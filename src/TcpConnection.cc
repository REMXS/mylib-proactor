

#include "TcpConnection.h"
#include "IoUringLoop.h"

void TcpConnection::sendInLoop(std::string data)
{
    //向缓冲区中追加数据
    write_context_.output_buffer_.append(std::move(data));

    //如果数据高于低水位线同时检查write_context中没有已经发送的sqe就发送，否则不发送
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
    loop_.submitReadMultishut(r_ctx);
}

SendDataAwaiter TcpConnection::send(std::string data)
{
    return SendDataAwaiter(this,std::move(data));
}

bool SendDataAwaiter::await_ready()
{
    //判断是否在当前的线程中，如果不在，直接挂起
    if(!conn_->loop_.isInLoopThread()) return false;

    //如果在loop线程，直接追加数据并提交任务
    conn_->sendInLoop(std::move(data_));

    //然后检查水位线，如果超过高水位线，就挂起
    return !conn_->write_context_.overLoad();
}

void SendDataAwaiter::await_suspend(std::coroutine_handle<> h)
{
    conn_->write_context_.write_handle_ = h;
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
        });
    }
}

//返回输出缓冲区是否出错，正确为true，错误为false
bool SendDataAwaiter::await_resume()
{
    return !conn_->write_context_.isError();
}
