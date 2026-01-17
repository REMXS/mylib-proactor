

#include "TcpConnection.h"
#include "IoUringLoop.h"
#include "Logger.h"

void TcpConnection::handleClose()
{
    if(!closing_)
    {
        auto ptr = shared_from_this();
        LOG_DEBUG("the connection is closing fd= %d",sock_.fd());
        closing_ = true;
        sock_.close();

        /*如果业务协程并没有被read/write context阻塞挂起，则业务协程可能在其它线程中运行，这个时候不能直接
          销毁协程，会出现未定义行为，要等到业务协程执行完毕，要读取或者是发送数据的时候在调用awaiter时
          检查连接是否关闭，如果关闭则直接退出，然后向对应loop中加入销毁协程的任务*/
        if(read_context_.read_handle_||write_context_.write_handle_)
        {
            task_handle_.destroy();
        }
        //将相关的资源置空
        read_context_.read_handle_ = nullptr;
        write_context_.write_handle_ = nullptr;

        if(close_callback_)
        {
            close_callback_(ptr);
        }
    }
    else
    {
        task_handle_.destroy();
    }
}

void TcpConnection::sendInLoop(std::string data)
{
    //向缓冲区中追加数据
    write_context_.output_buffer_.append(std::move(data));

    //如果检查write_context中没有已经发送的sqe就发送，否则已经有一个sqe发出去了，所以不发送
    if(!write_context_.is_sending_)
    {
        //先把智能指针赋值，保证生命周期，再发送数据
        write_context_.holder_ = shared_from_this();
        write_context_.flush();
        // is_sending_ 在 flush 中已经被设置为 true 了，这里可以省略，或者保持一致性
        write_context_.is_sending_ = true;
    }
}

void TcpConnection::submitWrite(WriteContext *w_ctx)
{
    loop_.submitWriteMsg(w_ctx);
}

void TcpConnection::submitRead(ReadContext *r_ctx)
{
    read_context_.holder_ = shared_from_this();//设置holder
    loop_.submitReadMultishut(r_ctx);           //提交任务
    read_context_.status_ = ReadContext::ReadStatus::READING;//设置状态
}

void TcpConnection::submitCancel(ReadContext *r_ctx)
{
    loop_.submitCancel(r_ctx);
}

TcpConnection::TcpConnection(
    std::string name, 
    IoUringLoop&loop, 
    int sockfd, 
    InetAddress local_addr, 
    InetAddress peer_addr, 
    size_t input_high_water_mark, 
    size_t input_high_water_mark_chunk, 
    size_t out_put_high_water_mark
    )
    :name_(name)
    ,loop_(loop)
    ,task_handle_(nullptr)
    ,sock_(sockfd)
    ,closing_(false)
    ,local_addr_(std::move(local_addr))
    ,peer_addr_(std::move(peer_addr))
    ,read_context_(input_high_water_mark,input_high_water_mark_chunk,sockfd,loop.getInputPool())
    ,write_context_(out_put_high_water_mark,sockfd)
{
    LOG_INFO("new TCP connection created, name=%s, fd=%d",name_.c_str(),sockfd)
    sock_.setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TCP connection destroyed, name=%s, fd=%d",name_.c_str(),sock_.fd());
}

SendDataAwaiter TcpConnection::send(std::string data)
{
    return SendDataAwaiter(this,std::move(data));
}

RecvDataAwaiter TcpConnection::PrepareToRead()
{
    return RecvDataAwaiter(this);
}

std::string TcpConnection::read(size_t size)
{
    return read_context_.input_buffer_.removeAsString(size);
}

std::pair<char *, size_t> TcpConnection::peek()
{
    return read_context_.input_buffer_.peek();
}

void TcpConnection::retrieve(size_t size)
{
    return read_context_.input_buffer_.retrieve(size);
}

void TcpConnection::Established(Task<> task_handle)
{
    task_handle_ = std::move(task_handle);
    //唤醒协程，因为task的initial_suspend 是suspend_always的
    task_handle_.resume();
}

bool RecvDataAwaiter::await_ready()
{
    //如果连接已经关闭，直接返回
    if(conn_->closing()) return true;

    //判断是否在当前的线程中，如果不在，直接挂起
    if(!conn_->loop_.isInLoopThread()) return false;
    //如果在loop线程，判断是否有数据可读，如果没有，就挂起
    return !conn_->read_context_.isEmpty();
}

void RecvDataAwaiter::await_suspend(std::coroutine_handle<> h)
{
    //如果不在当前的任务队列向loop的任务队列提交任务
    if(!conn_->loop_.isInLoopThread())
    {
        conn_->loop_.queueInLoop([h,this](){
            if(conn_->read_context_.isEmpty())
            {
                conn_->read_context_.read_handle_ = h;
                if(conn_->read_context_.status_== ReadContext::ReadStatus::STOPED){
                    conn_->submitRead(&conn_->read_context_);
                }
            }
            else
            {
                h.resume();
            }
        });
    }
    //如果在当前的线程且触发了这个函数，就代表输入缓冲区中无数据，提交读任务
    else
    {
        //这里协程挂起了，只要是协程挂起就要把handle交到一个地方以防无法唤醒
        conn_->read_context_.read_handle_ = h;
        //如果没有在提交的任务，就提交任务
        if(conn_->read_context_.status_== ReadContext::ReadStatus::STOPED)
        {
            conn_->submitRead(&conn_->read_context_);
        }
    }
}

int RecvDataAwaiter::await_resume()
{
    if(conn_->read_context_.is_error_) return -1;
    if(conn_->closing())
    {
        conn_->loop_.queueInLoop([conn_=conn_->getSharedPtr()](){conn_->Destroyed();});
    }
    return conn_->read_context_.input_buffer_.getTotalLen();
}

bool SendDataAwaiter::await_ready()
{
    //如果连接已经关闭，直接返回
    if(conn_->closing()) return true;
    
    //判断是否在当前的线程中，如果不在，直接挂起
    if(!conn_->loop_.isInLoopThread()) return false;

    //如果在loop线程，直接追加数据并提交任务
    conn_->sendInLoop(std::move(data_));

    //然后检查水位线，如果超过高水位线，就挂起
    return !conn_->write_context_.overLoad();
}

void SendDataAwaiter::await_suspend(std::coroutine_handle<> h)
{
    //如果不在当前的任务队列向loop的任务队列提交任务
    if(!conn_->loop_.isInLoopThread())
    {
        conn_->loop_.queueInLoop([h,this](){
            //添加数据
            conn_->sendInLoop(std::move(data_));

            //检查水位线,如果高于水位线不恢复执行，否则恢复执行
            if(!conn_->write_context_.overLoad()){
                h.resume();
            }
            else{
                //如果不在当前的线程，切换到loop的线程中再赋值，因为write_context_的数据访问必须是顺序的
                //如果可以恢复就不要赋值，只有在准备挂起的时候再赋值，否则可能会导致协程的唤起顺序混乱
                conn_->write_context_.write_handle_ = h;
            }
        });
    }
    else
    {
        conn_->write_context_.write_handle_ = h;
        LOG_DEBUG("SendDataAwaiter high water mark triggered!");
    }
}

//返回输出缓冲区是否出错，正确为true，错误为false
bool SendDataAwaiter::await_resume()
{
    if(conn_->closing())
    {
        conn_->loop_.queueInLoop([conn_=conn_->getSharedPtr()](){conn_->Destroyed();});
    }
    return !conn_->write_context_.isError();
}

