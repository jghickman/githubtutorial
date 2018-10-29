//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/task.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/task.inl,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Task
*/
inline
Task::Task(Handle h)
    : coro{h}
{
}


inline
Task::Task(Task&& other)
{
    steal(&other);
}


inline
Task::~Task()
{
    destroy(this);
}


inline void
Task::destroy(Task* taskp)
{
    if (taskp->coro)
        taskp->coro.destroy();
}


inline Task::Handle
Task::handle() const
{
    return coro;
}


inline Task&
Task::operator=(Task&& other)
{
    destroy(this);
    steal(&other);
    return *this;
}


inline void
Task::run()
{
    /*
        A Task that doesn't complete has placed itself on a waitlist so,
        in that case we relinquish coroutine ownership to that list.
    */
    coro.resume();
    if (!coro.done())
        coro = nullptr;
}


inline void
Task::steal(Task* otherp)
{
    coro = otherp->coro;
    otherp->coro = nullptr;
}


inline bool
operator==(const Task& x, const Task& y)
{
    return x.coro == y.coro;
}


/*
    Task Promise
*/
inline Task::Final_suspend
Task::Promise::final_suspend() const
{
    return Final_suspend{};
}


inline Task
Task::Promise::get_return_object()
{
    return Task(Handle::from_promise(*this));
}


inline Task::Initial_suspend
Task::Promise::initial_suspend() const
{
    return Initial_suspend{};
}


template<Channel_size N>
inline bool
Task::Promise::select(Channel_operation (&ops)[N], Task::Handle task)
{
    return select(begin(ops), end(ops), task);
}


inline bool
Task::Promise::select(Channel_size pos)
{
    return selenqco.put(pos);
}


template<Channel_size N>
optional<Channel_size>
Task::Promise::try_select(Channel_operation (&ops)[N])
{
    Channel_operation*  first = begin(ops);
    Channel_operation*  last = end(ops);
    Channel_sort        chansort{first, last};
    Channel_locks       chanlocks{first, last};

    return select_ready(first, last);
}


/*
    Channel Buffer
*/
template<class T>
inline
Channel<T>::Buffer::Buffer(Channel_size maxsize)
    : sizemax{maxsize >= 0 ? maxsize : 0}
{
    assert(maxsize >= 0);
}


template<class T>
inline bool
Channel<T>::Buffer::is_empty() const
{
    return q.empty();
}


template<class T>
inline bool
Channel<T>::Buffer::is_full() const
{
    return size() == max_size();
}


template<class T>
inline Channel_size
Channel<T>::Buffer::max_size() const
{
    return sizemax;
}


template<class T>
inline void
Channel<T>::Buffer::pop(T* valuep)
{
    using std::move;

    *valuep = move(q.front());
    q.pop();
}


template<class T>
inline void
Channel<T>::Buffer::pop(optional<T>* valuep)
{
    using std::move;

    *valuep = move(q.front());
    q.pop();
}


template<class T>
template<class U>
inline void
Channel<T>::Buffer::push(U&& value)
{
    using std::move;

    q.push(move(value));
}


template<class T>
inline Channel_size
Channel<T>::Buffer::size() const
{
    return static_cast<Channel_size>(q.size());
}


/*
    Channel Wait Queue
*/
template<class T>
template<class U>
inline typename Channel<T>::Wait_queue<U>::Iterator
Channel<T>::Wait_queue<U>::end()
{
    return ws.end();
}


template<class T>
template<class U>
inline void
Channel<T>::Wait_queue<U>::erase(Iterator p)
{
    ws.erase(p);
}


template<class T>
template<class U>
inline typename Channel<T>::Wait_queue<U>::Iterator
Channel<T>::Wait_queue<U>::find(Task::Handle task, Channel_size selpos)
{
    return std::find_if(ws.begin(), ws.end(), operation_eq(task, selpos));
}


template<class T>
template<class U>
inline bool
Channel<T>::Wait_queue<U>::is_found(const Waiter& w) const
{
    using std::find;

    const auto p = find(ws.begin(), ws.end(), w);
    return p != ws.end();
}


template<class T>
template<class U>
inline bool
Channel<T>::Wait_queue<U>::is_empty() const
{
    return ws.empty();
}


template<class T>
template<class U>
inline U
Channel<T>::Wait_queue<U>::pop()
{
    const U w = ws.front();

    ws.pop_front();
    return w;
}


template<class T>
template<class U>
inline void
Channel<T>::Wait_queue<U>::push(const Waiter& w)
{
    ws.push_back(w);
}


/*
    Channel Waiting Receiver
*/
template<class T>
inline
Channel<T>::Waiting_receive::Waiting_receive(Task::Handle tsk, Channel_size selpos, T* valuep)
    : taskh{tsk}
    , pos{selpos}
    , valp{valuep}
    , threadp{nullptr}
{
    assert(tsk);
    assert(valuep);
}


template<class T>
inline
Channel<T>::Waiting_receive::Waiting_receive(Condition_variable* waiterp, T* valuep)
    : valp{valuep}
    , threadp{waiterp}
{
    assert(waiterp);
    assert(valuep);
}


template<class T>
template<class U>
inline bool
Channel<T>::Waiting_receive::dequeue(U* valuep)
{
    using std::move;

    bool is_dequeued = false;

    if (taskh) {
        if (taskh.promise().select(pos)) {
            *valp = move(*valuep);
            scheduler.resume(taskh);
            is_dequeued = true;
        }
    } else {
        *valp = move(*valuep);
        threadp->notify_one();
        is_dequeued = true;
    }

    return is_dequeued;
}

template<class T>
inline Channel_size
Channel<T>::Waiting_receive::select_position() const
{
    return pos;
}


template<class T>
inline Task::Handle
Channel<T>::Waiting_receive::task() const
{
    return taskh;
}


/*
    Channel Waiting Sender
*/
template<class T>
inline
Channel<T>::Waiting_send::Waiting_send(Task::Handle tsk, Channel_size selpos, const T* rvaluep)
    : taskh{tsk}
    , pos{selpos}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , threadp{nullptr}
{
    assert(tsk);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Waiting_send::Waiting_send(Task::Handle tsk, Channel_size selpos, T* lvaluep)
    : taskh{tsk}
    , pos{selpos}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , threadp{nullptr}
{
    assert(tsk);
    assert(lvaluep);
}


template<class T>
inline
Channel<T>::Waiting_send::Waiting_send(Condition_variable* waiterp, const T* rvaluep)
    : rvalp{rvaluep}
    , lvalp{nullptr}
    , threadp{waiterp}
{
    assert(waiterp);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Waiting_send::Waiting_send(Condition_variable* waiterp, T* lvaluep)
    : rvalp{nullptr}
    , lvalp{lvaluep}
    , threadp{waiterp}
{
    assert(waiterp);
    assert(lvaluep);
}


template<class T>
template<class U>
bool
Channel<T>::Waiting_send::dequeue(U* recvbufp)
{
    bool is_dequeued = false;

    if (taskh) {
        if (taskh.promise().select(pos)) {
            move(lvalp, rvalp, recvbufp);
            scheduler.resume(taskh);
            is_dequeued = true;
        }
    } else {
        move(lvalp, rvalp, recvbufp);
        threadp->notify_one();
        is_dequeued = true;
    }

    return is_dequeued;
}


template<class T>
template<class U>
inline void
Channel<T>::Waiting_send::move(T* lvalp, const T* rvalp, U* recvbufp)
{
    *recvbufp = lvalp ? std::move(*lvalp) : *rvalp;
}


template<class T>
inline void
Channel<T>::Waiting_send::move(T* lvalp, const T* rvalp, Buffer* bufp)
{
    if (lvalp)
        bufp->push(std::move(*lvalp));
    else
        bufp->push(*rvalp);
}


template<class T>
inline Channel_size
Channel<T>::Waiting_send::select_position() const
{
    return pos;
}


template<class T>
inline Task::Handle
Channel<T>::Waiting_send::task() const
{
    return taskh;
}


/*
    Channel
*/
template<class T>
inline
Channel<T>::Channel()
{
}


template<class T>
inline
Channel<T>::Channel(Impl_ptr p)
    : pimpl{std::move(p)}
{
}


template<class T>
inline Channel_size
Channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline Channel_operation
Channel<T>::make_receive(T* valuep) const
{
    return pimpl->make_receive(valuep);
}


template<class T>
inline Channel_operation
Channel<T>::make_send(const T& value) const
{
    return pimpl->make_send(&value);
}


template<class T>
inline Channel_operation
Channel<T>::make_send(T&& value) const
{
    return pimpl->make_send(&value);
}


template<class T>
inline Channel<T>&
Channel<T>::operator=(Channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


template<class T>
inline
Channel<T>::operator bool() const
{
    return pimpl ? true : false;
}


template<class T>
inline typename Channel<T>::Receive_awaitable
Channel<T>::receive() const
{
    return pimpl->receive();
}


template<class T>
inline typename Channel<T>::Send_awaitable
Channel<T>::send(const T& value) const
{
    return pimpl->awaitable_send(&value);
}


template<class T>
inline typename Channel<T>::Send_awaitable
Channel<T>::send(T&& value) const
{
    return pimpl->awaitable_send(&value);
}


template<class T>
inline Channel_size
Channel<T>::size() const
{
    return pimpl->size();
}


template<class T>
inline T
Channel<T>::sync_receive() const
{
    return pimpl->sync_receive();
}


template<class T>
inline void
Channel<T>::sync_send(const T& value) const
{
    return pimpl->sync_send(&value);
}


template<class T>
inline void
Channel<T>::sync_send(T&& value) const
{
    return pimpl->sync_send(&value);
}


template<class T>
inline optional<T>
Channel<T>::try_receive() const
{
    return pimpl->try_receive();
}


template<class T>
inline bool
Channel<T>::try_send(const T& value) const
{
    return pimpl->try_send(value);
}


template<class T>
inline bool
operator==(const Channel<T>& x, const Channel<T>& y)
{
    return x.pimpl != y.pimpl;
}


template<class T>
inline bool
operator< (const Channel<T>& x, const Channel<T>& y)
{
    return x.pimpl < y.pimpl;
}


template<class T>
inline void
swap(Channel<T>& x, Channel<T>& y)
{
    using std::swap;
    swap(x.pimpl, y.pimpl);
}


/*
    Channel Implementation
*/
template<class T>
inline
Channel<T>::Impl::Impl(Channel_size n)
    : buffer{n}
{
}


template<class T>
typename Channel<T>::Receive_awaitable
Channel<T>::Impl::awaitable_receive()
{
    return Receive_awaitable(this);
}


template<class T>
template<class U>
typename Channel<T>::Send_awaitable
Channel<T>::Impl::awaitable_send(U* valuep)
{
    return Send_awaitable(this, valuep);
}


template<class T>
Channel_size
Channel<T>::Impl::capacity() const
{
    return buffer.capacity();
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue(Receive_queue* waitqp, U* sendbufp)
{
    bool is_dequeued = false;

    while (!(is_dequeued || waitqp->is_empty())) {
        Waiting_receive receiver = waitqp->pop();
        is_dequeued = receiver.dequeue(sendbufp);
    }

    return is_dequeued;
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue(Send_queue* waitqp, U* recvbufp)
{
    bool is_dequeued = false;

    while (!(is_dequeued || waitqp->is_empty())) {
        Waiting_send sender = waitqp->pop();
        is_dequeued = sender.dequeue(recvbufp);
    }

    return is_dequeued;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::dequeue(U* waitqp, Task::Handle task, Channel_size selpos)
{
    auto wp = waitqp->find(task, selpos);
    if (wp != waitqp->end())
        waitqp->erase(wp);
}


template<class T>
void
Channel<T>::Impl::dequeue_receive(Task::Handle task, Channel_size selpos)
{
    dequeue(&recvq, task, selpos);
}


template<class T>
void
Channel<T>::Impl::dequeue_send(Task::Handle task, Channel_size selpos)
{
    dequeue(&sendq, task, selpos);
}


template<class T>
void
Channel<T>::Impl::enqueue_receive(Task::Handle task, Channel_size selpos, void* valuep)
{
    enqueue_receive(task, selpos, static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::enqueue_receive(Task::Handle task, Channel_size selpos, T* valuep)
{
    const Waiting_receive r{task, selpos, valuep};
    recvq.push(r);
}


template<class T>
void
Channel<T>::Impl::enqueue_send(Task::Handle task, Channel_size selpos, const void* rvaluep)
{
    enqueue_send(task, selpos, static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::enqueue_send(Task::Handle task, Channel_size selpos, void* lvaluep)
{
    enqueue_send(task, selpos, static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::enqueue_send(Task::Handle task, Channel_size selpos, U* valuep)
{
    const Waiting_send s{task, selpos, valuep};
    sendq.push(s);
}


template<class T>
bool
Channel<T>::Impl::is_receive_ready() const
{
    return !(buffer.is_empty() && recvq.is_empty());
}


template<class T>
bool
Channel<T>::Impl::is_send_ready() const
{
    return !(buffer.is_full() && recvq.is_empty());
}


template<class T>
void
Channel<T>::Impl::lock()
{
    mutex.lock();
}


template<class T>
Channel_operation
Channel<T>::Impl::make_receive(T* valuep)
{
    return Channel_operation(this, valuep, Channel_operation::receive);
}


template<class T>
Channel_operation
Channel<T>::Impl::make_send(T* valuep)
{
    return Channel_operation(this, valuep, Channel_operation::send);
}


template<class T>
Channel_operation
Channel<T>::Impl::make_send(const T* valuep)
{
    return Channel_operation(this, valuep);
}


template<class T>
void
Channel<T>::Impl::receive_ready(void* valuep)
{
    receive_ready(static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::receive_ready(T* valuep)
{
    if (!dequeue(&sendq, valuep))
        buffer.pop(valuep);
}


template<class T>
void
Channel<T>::Impl::send_ready(const void* rvaluep)
{
    send_ready(static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::send_ready(void* lvaluep)
{
    send_ready(static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::send_ready(U* valuep)
{
    if (!dequeue(&recvq, valuep))
        buffer.push(move(*valuep));
}


template<class T>
Channel_size
Channel<T>::Impl::size() const
{
    return buffer.size();
}


template<class T>
T
Channel<T>::Impl::sync_receive()
{
    T       value;
    Lock    lock{mutex};

    if (!dequeue(&sendq, &value)) {
        if (!buffer.is_empty())
            buffer.pop(&value);
        else
            wait_for_sender(lock, &recvq, &value);
    }

    return value;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::sync_send(U* valuep)
{
    bool is_done = false;
    Lock lock{mutex};

    if (!dequeue(&recvq, valuep)) {
        if (!buffer.is_full())
            buffer.push(move(*valuep));
        else
            wait_for_receiver(lock, &sendq, valuep);
    }
}


template<class T>
optional<T>
Channel<T>::Impl::try_receive()
{
    optional<T> value;
    Lock        lock{mutex};

    if (!dequeue(&sendq, &value)) {
        if (!buffer.is_empty())
            buffer.pop(&value);
    }

    return value;
}


template<class T>
bool
Channel<T>::Impl::try_send(const T& value)
{
    bool is_sent{false};
    Lock lock{mutex};

    if (dequeue(&recvq, &value))
        is_sent = true;
    else if (!buffer.is_full()) {
        buffer.push(value);
        is_sent = true;
    }

    return is_sent;
}


template<class T>
void
Channel<T>::Impl::unlock()
{
    mutex.unlock();
}


template<class T>
template<class U>
void
Channel<T>::Impl::wait_for_receiver(Lock& lock, Send_queue* waitqp, U* sendbufp)
{
    Condition_variable ready;
    const Waiting_send ownsend{&ready, sendbufp};

    // Enqueue our send and wait for a receiver to remove it.
    waitqp->push(ownsend);
    ready.wait(lock, [&]{ return !waitqp->is_found(ownsend); });
}


template<class T>
void
Channel<T>::Impl::wait_for_sender(Lock& lock, Receive_queue* waitqp, T* recvbufp)
{
    Condition_variable      ready;
    const Waiting_receive   ownrecv{&ready, recvbufp};

    // Enqueue our receive and wait for a sender to remove it.
    waitqp->push(ownrecv);
    ready.wait(lock, [&]{ return !waitqp->is_found(ownrecv); });
}


/*
    Channel Receive Awaitable
*/
template<class T>
inline
Channel<T>::Receive_awaitable::Receive_awaitable(Impl* channelp)
    : receive{channelp->make_receive(&value)}
{
}


template<class T>
inline bool
Channel<T>::Receive_awaitable::await_ready()
{
    return false;
}


template<class T>
inline T&&
Channel<T>::Receive_awaitable::await_resume()
{
    return std::move(value);
}


template<class T>
inline bool
Channel<T>::Receive_awaitable::await_suspend(Task::Handle task)
{
    return !task.promise().select(receive, task);
}


/*
    Channel Send Awaitable
*/
template<class T>
template<class U>
inline
Channel<T>::Send_awaitable::Send_awaitable(Impl* channelp, U* valuep)
    : send{channelp->make_send(valuep)}
{
}


template<class T>
inline bool
Channel<T>::Send_awaitable::await_ready()
{
    return false;
}


template<class T>
inline void
Channel<T>::Send_awaitable::await_resume()
{
}


template<class T>
bool
Channel<T>::Send_awaitable::await_suspend(Task::Handle task)
{
    return !task.promise().select(send, task);
}


/*
    Receive Channel
*/
template<class T>
inline
Receive_channel<T>::Receive_channel()
{
}


template<class T>
inline
Receive_channel<T>::Receive_channel(const Channel<T>& chan)
    : pimpl(chan.pimpl)
{
}


template<class T>
inline Channel_size
Receive_channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline Channel_operation
Receive_channel<T>::make_receive(T* valuep)
{
    return pimpl->make_receive(valuep);
}


template<class T>
inline Receive_channel<T>&
Receive_channel<T>::operator=(Receive_channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


template<class T>
inline Receive_channel<T>&
Receive_channel<T>::operator=(const Channel<T>& other)
{
    pimpl = other.pimpl;
    return *this;
}


template<class T>
inline
Receive_channel<T>::operator bool() const
{
    return pimpl ? true : false;
}


template<class T>
inline typename Receive_channel<T>::Awaitable
Receive_channel<T>::receive() const
{
    return pimpl->awaitable_receive();
}


template<class T>
inline Channel_size
Receive_channel<T>::size() const
{
    return pimpl->size();
}


template<class T>
inline T
Receive_channel<T>::sync_receive() const
{
    return pimpl->sync_receive();
}


template<class T>
inline optional<T>
Receive_channel<T>::try_receive() const
{
    return pimpl->try_receive();
}


template<class T>
inline bool
operator==(const Receive_channel<T>& x, const Receive_channel<T>& y)
{
    return x.pimpl != y.pimpl;
}


template<class T>
inline bool
operator< (const Receive_channel<T>& x, const Receive_channel<T>& y)
{
    return x.pimpl < y.pimpl;
}


template<class T>
inline void
swap(Receive_channel<T>& x, Receive_channel<T>& y)
{
    using std::swap;
    swap(x.pimpl, y.pimpl);
}


/*
    Send Channel
*/
template<class T>
inline
Send_channel<T>::Send_channel()
{
}


template<class T>
inline
Send_channel<T>::Send_channel(const Channel<T>& other)
    : pimpl{other.pimpl}
{
}


template<class T>
inline Channel_size
Send_channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline Channel_operation
Send_channel<T>::make_send(const T& value) const
{
    return pimpl->make_send(&value);
}


template<class T>
inline Channel_operation
Send_channel<T>::make_send(T&& value) const
{
    return pimpl->make_send(&value);
}


template<class T>
inline Send_channel<T>&
Send_channel<T>::operator=(Send_channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


template<class T>
inline Send_channel<T>&
Send_channel<T>::operator=(const Channel<T>& other)
{
    pimpl = other.pimpl;
    return *this;
}


template<class T>
inline
Send_channel<T>::operator bool() const
{
    return pimpl ? true : false;
}


template<class T>
inline typename Send_channel<T>::Awaitable
Send_channel<T>::send(const T& value) const
{
    return pimpl->awaitable_send(&value);
}


template<class T>
inline typename Send_channel<T>::Awaitable
Send_channel<T>::send(T&& value) const
{
    return pimpl->awaitable_send(&value);
}


template<class T>
inline Channel_size
Send_channel<T>::size() const
{
    return pimpl->size();
}


template<class T>
inline void
Send_channel<T>::sync_send(const T& value) const
{
    pimpl->sync_send(value);
}


template<class T>
inline void
Send_channel<T>::sync_send(T&& value) const
{
    using std::move;
    pimpl->sync_send(move(value));
}


template<class T>
inline bool
Send_channel<T>::try_send(const T& value) const
{
    return pimpl->try_send(value);
}


template<class T>
inline bool
operator==(const Send_channel<T>& x, const Send_channel<T>& y)
{
    return x.pimpl == y.pimpl;
}


template<class T>
inline bool
operator< (const Send_channel<T>& x, const Send_channel<T>& y)
{
    return x.pimpl < y.pimpl;
}


template<class T>
inline void
swap(Send_channel<T>& x, Send_channel<T>& y)
{
    using std::swap;
    swap(x.pimpl, y.pimpl);
}


/*
    Channel Select Awaitable
*/
inline
Channel_select_awaitable::Channel_select_awaitable(Channel_operation* begin, Channel_operation* end)
    : first{begin}
    , last{end}
{
}


inline bool
Channel_select_awaitable::await_ready()
{
    return false;
}


inline Channel_size
Channel_select_awaitable::await_resume()
{
    return promisep->selected();
}


inline bool
Channel_select_awaitable::await_suspend(Task::Handle task)
{
    promisep = &task.promise();
    return !promisep->select(first, last, task);
}


/*
    Channel Select
*/
template<Channel_size N>
inline Channel_select_awaitable
select(Channel_operation (&ops)[N])
{
    return Channel_select_awaitable(begin(ops), end(ops));
}


/*
    Non-Blocking Channel Select
*/
template<Channel_size N>
inline optional<Channel_size>
try_select(Channel_operation (&ops)[N])
{
    return Task::Promise::try_select(ops);
}


/*
    Channel
*/
template<class T>
Channel<T>
make_channel(Channel_size n)
{
    return std::make_shared<Channel<T>::Impl>(n);
}


/*
    Future Awaitable
*/
template<class T>
inline
Future<T>::Awaitable::Awaitable(Future* fp)
    : selfp(fp)
{
}


template<class T>
inline bool
Future<T>::Awaitable::await_ready()
{
    assert(selfp->is_valid());
    return false;
}


template<class T>
inline T
Future<T>::Awaitable::await_resume()
{
    return selfp->resume(task);
}


template<class T>
inline bool
Future<T>::Awaitable::await_suspend(Task::Handle h)
{
    task = h;
    return selfp->suspend(task);
}


/*
    Future     
*/
template<class T>
inline
Future<T>::Future()
{
}


template<class T>
inline
Future<T>::Future(Receive_channel<T> vc, Receive_channel<exception_ptr> ec)
    : vchan{std::move(vc)}
    , echan{std::move(ec)}
{
}


template<class T>
inline typename Future<T>::Awaitable
Future<T>::get()
{
    return this;
}


template<class T>
inline bool
Future<T>::is_valid() const
{
    return vchan && echan;
}


template<class T>
T
Future<T>::resume(Task::Handle task)
{
    using std::move;
    using std::rethrow_exception;

    if (task.promise().selected() == errorpos)
        rethrow_exception(ep);

    return move(v);
}


template<class T>
bool
Future<T>::suspend(Task::Handle task)
{
    Channel_operation ops[] = {
          vchan.make_receive(&v)
        , echan.make_receive(&ep)
    };

    return !task.promise().select(ops, task);
}


template<class T>
inline void
swap(Future<T>& x, Future<T>& y)
{
    using std::swap;

    swap(x.vchan, y.vchan);
    swap(x.echan, y.echan);
    swap(x.v, y.v);
    swap(x.ep, y.ep);
}


template<class T>
inline bool
operator==(const Future<T>& x, const Future<T>& y)
{
    if (x.vchan != y.echan) return false;
    if (x.echan != y.echan) return false;
    return true;
}


/*
    Task Launcher
*/
template<class TaskFun, class... Args>
inline void
start(TaskFun f, Args&&... args)
{
    using std::forward;
    using std::move;

    Task task = f(forward<Args>(args)...);
    scheduler.submit(move(task));
}


template<class Fun, class... Args>
Future<std::result_of_t<Fun&&(Args&&...)>>
async(Fun f, Args&&... args)
{
    using std::current_exception;
    using std::forward;
    using std::move;
    using Result            = std::result_of_t<Fun&&(Args&&...)>;
    using Result_channel    = Channel<Result>;
    using Error_channel     = Channel<exception_ptr>;
    using Result_sender     = Send_channel<Result>;
    using Error_sender      = Send_channel<exception_ptr>;

    Result_channel  r       = make_channel<Result>(1);
    Error_channel   e       = make_channel<exception_ptr>(1);
    auto            taskfun = [](Result_sender r, Error_sender e, Fun f, Args&&... args) -> Task {
        exception_ptr ep;

        try {
            co_await r.send(f(forward<Args>(args)...));
        } catch (...) {
            ep = current_exception();
        }

        if (ep) co_await e.send(ep);
    };

    start(move(taskfun), r, e, move(f), forward<Args>(args)...);
    return Future<Result>{r, e};
}


}  // Concurrency
}  // Isptech

