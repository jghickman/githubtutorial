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
    Task Future Selection Wait
*/
inline
Task::Future_selection::Wait::Wait()
{
}


inline
Task::Future_selection::Wait::Wait(Channel_base* chanp, bool* rdyflagp, Channel_size pos)
    : channelp{chanp}
    , readyp{rdyflagp}
    , waitpos{pos}
{
}


inline Channel_base*
Task::Future_selection::Wait::channel() const
{
    return channelp;
}


inline void
Task::Future_selection::Wait::complete() const
{
    *readyp = true;
}


inline void
Task::Future_selection::Wait::dequeue(Task::Handle task) const
{
    channelp->dequeue_receivable_wait(task, waitpos);
}


inline void
Task::Future_selection::Wait::enqueue(Task::Handle task) const
{
    channelp->enqueue_receivable_wait(task, waitpos);
}


inline bool
Task::Future_selection::Wait::is_ready() const
{
    return channelp->is_receive_ready();
}


inline void
Task::Future_selection::Wait::lock_channel() const
{
    channelp->lock();
}


inline Channel_size
Task::Future_selection::Wait::position() const
{
    return waitpos;
}


inline void
Task::Future_selection::Wait::unlock_channel() const
{
    channelp->unlock();
}


/*
    Task Future Selection Channel Locks
*/
Task::Future_selection::Channel_locks::Channel_locks(const Wait_vector* wsp)
    : waitsp(wsp)
{
    for (const Wait& w : *waitsp)
        w.lock_channel();
}


Task::Future_selection::Channel_locks::~Channel_locks()
{
    for (auto& w : *waitsp)
        w.unlock_channel();
}


/*
    Task Future Selection Transform
*/
template<class T>
Task::Future_selection::Transform<T>::Transform(const Future<T>* first, const Future<T>* last, Wait_vector* waitsp)
{
    waitsp->resize(2 * (last - first));
    transform(first, last, waitsp->begin());
    sort_channels(waitsp);
}


template<class T>
inline Task::Future_selection::Wait
Task::Future_selection::Transform<T>::error_wait(const Future<T>* fp, Channel_size pos)
{
    return Wait(fp->error_channel(), fp->ready_flag(), pos);
}


template<class T>
inline void
Task::Future_selection::Transform<T>::sort_channels(Wait_vector* waitsp)
{
    std::sort(waitsp->begin(), waitsp->end(), [](const Wait& x, const Wait& y) {
        return x.channel() < y.channel();
    });
}


template<class T>
void
Task::Future_selection::Transform<T>::transform(const Future<T>* first, const Future<T>* last, Wait_vector::iterator out)
{
    for (auto fp = first; fp != last; ++fp) {
        const auto fpos = fp - first;
        *out++ = value_wait(fp, fpos);
        *out++ = error_wait(fp, fpos);
    }
}


template<class T>
inline Task::Future_selection::Wait
Task::Future_selection::Transform<T>::value_wait(const Future<T>* fp, Channel_size pos)
{
    return Wait(fp->value_channel(), fp->ready_flag(), pos);
}


/*
    Task Future Selection
*/
Channel_size
Task::Future_selection::count_ready(const Wait_vector& ws)
{
    return count_if(ws.begin(), ws.end(), [](const Wait& w) {
        return w.is_ready();
    });
}


void
Task::Future_selection::enqueue(const Wait_vector& ws, Handle task)
{
    for (const Wait& w : ws)
        w.enqueue(task);
}


Task::Future_selection::Wait_vector::const_iterator
Task::Future_selection::pick_ready(const Wait_vector& waits, Channel_size nready)
{
    auto            readyp  = waits.end();
    Channel_size    n       = random(1, nready);

    for (auto wp = waits.begin(); wp != waits.end(); ++wp) {
        if (wp->is_ready() && --n == 0) {
            readyp = wp;
            break;
        }
    }

    return readyp;
}


optional<Channel_size>
Task::Future_selection::select_ready(const Wait_vector& ws)
{
    optional<Channel_size>  pos;
    const Channel_size      n = count_ready(ws);

    if (n > 0) {
        auto wp = pick_ready(ws, n);
        pos = wp->position();
        wp->complete();
    }

    return pos;
}


template<class T>
bool
Task::Future_selection::wait_any(const Future<T>* first, const Future<T>* last, Handle task)
{
    Transform<T>    transform(first, last, &waits);
    Channel_locks   lock(&waits);

    chosen = select_ready(waits);
    if (!chosen)
        enqueue(waits, task);

    return chosen ? true : false;
}


/*
    Task Promise
*/
inline
Task::Promise::Promise()
    : taskstat{Status::ready}
{
}


inline Task::Final_suspend
Task::Promise::final_suspend()
{
    taskstat = Status::done;
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
inline void
Task::Promise::select(Channel_operation (&ops)[N])
{
    select(begin(ops), end(ops));
}


inline void
Task::Promise::select(Channel_operation* first, Channel_operation* last)
{
    const Handle    task{Handle::from_promise(*this)};
    Lock            lock{mutex};

    if (!channels.select(first, last, task))
        suspend(&lock);
}


inline bool
Task::Promise::select(Channel_size pos)
{
    const Lock lock{mutex};

    return channels.select(pos);
}


inline Channel_size
Task::Promise::selected()
{
    const Handle task = Handle::from_promise(*this);

    return channels.selected(task);
}


template<Channel_size N>
optional<Channel_size>
Task::Promise::try_select(Channel_operation(&ops)[N])
{
    return channels.try_select(ops);
}


template<class T>
void
Task::Promise::wait_all(const Future<T>* first, const Future<T>* last)
{
    const Handle    task{Handle::from_promise(*this)};
    Lock            lock{mutex};

    if (!futures.wait_all(first, last, task))
        suspend(&lock);
}


template<class T>
void
Task::Promise::wait_any(const Future<T>* first, const Future<T>* last)
{
    const Handle    task{Handle::from_promise(*this)};
    Lock            lock{mutex};

    if (!futures.wait_any(first, last, task))
        suspend(&lock);
}


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
    : coro{nullptr}
{
    swap(*this, other);
}


inline
Task::~Task()
{
    if (coro)
        coro.destroy();
}


inline Task::Handle
Task::handle() const
{
    return coro;
}


inline void
Task::make_ready()
{
    coro.promise().make_ready();
}


inline Task&
Task::operator=(Task&& other)
{
    swap(*this, other);
    return *this;
}


inline Task::Status
Task::resume()
{
    coro.resume();
    return coro.promise().status();
}


inline void
Task::unlock()
{
    coro.promise().unlock();
}


inline bool
operator==(const Task& x, const Task& y)
{
    return x.coro == y.coro;
}


inline void
swap(Task& x, Task& y)
{
    using std::swap;

    swap(x.coro, y.coro);
}


/*
    Channel Readable Waiter
*/
template<class T>
inline
Channel<T>::Readable_waiter::Readable_waiter()
{
}


template<class T>
inline
Channel<T>::Readable_waiter::Readable_waiter(Task::Handle task, Channel_size pos)
    : taskh{task}
    , waitpos{pos}
{
}


template<class T>
inline void
Channel<T>::Readable_waiter::notify() const
{
    return taskh.promise().notify_receive_ready(waitpos);
}


template<class T>
inline Channel_size
Channel<T>::Readable_waiter::position() const
{
    return waitpos;
}


template<class T>
inline Task::Handle
Channel<T>::Readable_waiter::task() const
{
    return taskh;
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
inline void
Channel<T>::Buffer::enqueue(const Readable_waiter& w)
{
    readers.push_back(w);
}


template<class T>
inline void
Channel<T>::Buffer::dequeue(const Readable_waiter& w)
{
    using std::find;

    auto wp = find(readers.begin(), readers.end(), w);
    if (wp != readers.end())
        readers.erase(wp);
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
void
Channel<T>::Buffer::push(U&& value)
{
    using std::move;

    q.push(move(value));

    if (!readers.empty()) {
        auto& waiter = readers.front();
        waiter.notify();
        readers.pop_front();
    }
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
inline typename Channel<T>::Waitqueue<U>::Iterator
Channel<T>::Waitqueue<U>::end()
{
    return ws.end();
}


template<class T>
template<class U>
inline void
Channel<T>::Waitqueue<U>::erase(Iterator p)
{
    ws.erase(p);
}


template<class T>
template<class U>
inline typename Channel<T>::Waitqueue<U>::Iterator
Channel<T>::Waitqueue<U>::find(Task::Handle task, Channel_size selpos)
{
    return std::find_if(ws.begin(), ws.end(), operation_eq(task, selpos));
}


template<class T>
template<class U>
inline bool
Channel<T>::Waitqueue<U>::is_found(const Waiter& w) const
{
    using std::find;

    const auto p = find(ws.begin(), ws.end(), w);
    return p != ws.end();
}


template<class T>
template<class U>
inline bool
Channel<T>::Waitqueue<U>::is_empty() const
{
    return ws.empty();
}


template<class T>
template<class U>
inline U
Channel<T>::Waitqueue<U>::pop()
{
    const U w = ws.front();

    ws.pop_front();
    return w;
}


template<class T>
template<class U>
inline void
Channel<T>::Waitqueue<U>::push(const Waiter& w)
{
    ws.push_back(w);
}


/*
    Channel Waiting Receiver
*/
template<class T>
inline
Channel<T>::Waiting_receiver::Waiting_receiver(Task::Handle tsk, Channel_size selpos, T* valuep)
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
Channel<T>::Waiting_receiver::Waiting_receiver(Condition_variable* waiterp, T* valuep)
    : valp{valuep}
    , threadp{waiterp}
{
    assert(waiterp);
    assert(valuep);
}


template<class T>
template<class U>
inline bool
Channel<T>::Waiting_receiver::dequeue(U* valuep) const
{
    using std::move;

    bool is_received = false;

    if (taskh) {
        if (taskh.promise().select(pos)) {
            *valp = move(*valuep);
            scheduler.resume(taskh);
            is_received = true;
        }
    } else {
        *valp = move(*valuep);
        threadp->notify_one();
        is_received = true;
    }

    return is_received;
}

template<class T>
inline Channel_size
Channel<T>::Waiting_receiver::select_position() const
{
    return pos;
}


template<class T>
inline Task::Handle
Channel<T>::Waiting_receiver::task() const
{
    return taskh;
}


/*
    Channel Waiting Sender
*/
template<class T>
inline
Channel<T>::Waiting_sender::Waiting_sender(Task::Handle tsk, Channel_size selpos, const T* rvaluep)
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
Channel<T>::Waiting_sender::Waiting_sender(Task::Handle tsk, Channel_size selpos, T* lvaluep)
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
Channel<T>::Waiting_sender::Waiting_sender(Condition_variable* waiterp, const T* rvaluep)
    : rvalp{rvaluep}
    , lvalp{nullptr}
    , threadp{waiterp}
{
    assert(waiterp);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Waiting_sender::Waiting_sender(Condition_variable* waiterp, T* lvaluep)
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
Channel<T>::Waiting_sender::dequeue(U* recvbufp) const
{
    bool is_sent = false;

    if (taskh) {
        if (taskh.promise().select(pos)) {
            move(lvalp, rvalp, recvbufp);
            scheduler.resume(taskh);
            is_sent = true;
        }
    } else {
        move(lvalp, rvalp, recvbufp);
        threadp->notify_one();
        is_sent = true;
    }

    return is_sent;
}


template<class T>
template<class U>
inline void
Channel<T>::Waiting_sender::move(T* lvalp, const T* rvalp, U* recvbufp)
{
    *recvbufp = lvalp ? std::move(*lvalp) : *rvalp;
}


template<class T>
inline void
Channel<T>::Waiting_sender::move(T* lvalp, const T* rvalp, Buffer* bufp)
{
    if (lvalp)
        bufp->push(std::move(*lvalp));
    else
        bufp->push(*rvalp);
}


template<class T>
inline Channel_size
Channel<T>::Waiting_sender::select_position() const
{
    return pos;
}


template<class T>
inline Task::Handle
Channel<T>::Waiting_sender::task() const
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
inline bool
Channel<T>::is_empty() const
{
    return pimpl->is_empty();
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
T
Channel<T>::Impl::blocking_receive()
{
    T       value;
    Lock    lock{mutex};

    if (!dequeue(&senders, &value)) {
        if (!buffer.is_empty())
            buffer.pop(&value);
        else
            wait_for_sender(&lock, &receivers, &value);
    }

    return value;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::blocking_send(U* valuep)
{
    Lock lock{mutex};

    if (!dequeue(&receivers, valuep)) {
        if (!buffer.is_full())
            buffer.push(move(*valuep));
        else
            wait_for_receiver(&lock, &senders, valuep);
    }
}


template<class T>
Channel_size
Channel<T>::Impl::capacity() const
{
    const Lock lock{mutex};
    return buffer.capacity();
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue(Receiver_waitqueue* qp, U* sendbufp)
{
    bool is_sent = false;

    while (!(is_sent || qp->is_empty())) {
        const Waiting_receiver receiver = qp->pop();
        is_sent = receiver.dequeue(sendbufp);
    }

    return is_sent;
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue(Sender_waitqueue* qp, U* recvbufp)
{
    bool is_received = false;

    while (!(is_received || qp->is_empty())) {
        const Waiting_sender sender = qp->pop();
        is_received = sender.dequeue(recvbufp);
    }

    return is_received;
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
Channel<T>::Impl::dequeue_receivable_wait(Task::Handle task, Channel_size pos)
{
    buffer.dequeue(Readable_waiter(task, pos));
}


template<class T>
void
Channel<T>::Impl::dequeue_receive(Task::Handle task, Channel_size selpos)
{
    dequeue(&receivers, task, selpos);
}


template<class T>
void
Channel<T>::Impl::dequeue_send(Task::Handle task, Channel_size selpos)
{
    dequeue(&senders, task, selpos);
}


template<class T>
void
Channel<T>::Impl::enqueue_receivable_wait(Task::Handle task, Channel_size pos)
{
    buffer.enqueue(Readable_waiter(task, pos));
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
    const Waiting_receiver r{task, selpos, valuep};
    receivers.push(r);
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
    const Waiting_sender s{task, selpos, valuep};
    senders.push(s);
}


template<class T>
bool
Channel<T>::Impl::is_empty() const
{
    const Lock lock{mutex};
    return buffer.is_empty();
}


template<class T>
bool
Channel<T>::Impl::is_receive_ready() const
{
    return !(buffer.is_empty() && receivers.is_empty());
}


template<class T>
bool
Channel<T>::Impl::is_send_ready() const
{
    return !(buffer.is_full() && receivers.is_empty());
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
    return Channel_operation(this, valuep, Channel_operation::Type::receive);
}


template<class T>
Channel_operation
Channel<T>::Impl::make_send(T* valuep)
{
    return Channel_operation(this, valuep, Channel_operation::Type::send);
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
    if (!dequeue(&senders, valuep))
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
    if (!dequeue(&receivers, valuep))
        buffer.push(move(*valuep));
}


template<class T>
Channel_size
Channel<T>::Impl::size() const
{
    Lock lock{mutex};
    return buffer.size();
}


template<class T>
optional<T>
Channel<T>::Impl::try_receive()
{
    optional<T> value;
    Lock        lock{mutex};

    if (!dequeue(&senders, &value)) {
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

    if (dequeue(&receivers, &value))
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
void
Channel<T>::Impl::wait_receivable(Task::Handle task, Channel_size pos)
{
    receivables.push_back(Readable_waiter(task, pos));
}


template<class T>
template<class U>
void
Channel<T>::Impl::wait_for_receiver(Lock* lockp, Sender_waitqueue* waitqp, U* sendbufp)
{
    Condition_variable      ready;
    const Waiting_sender    ownsend{&ready, sendbufp};

    // Enqueue our send and wait for a receiver to remove it.
    waitqp->push(ownsend);
    ready.wait(*lockp, [&]{ return !waitqp->is_found(ownsend); });
}


template<class T>
void
Channel<T>::Impl::wait_for_sender(Lock* lockp, Receiver_waitqueue* waitqp, T* recvbufp)
{
    Condition_variable      ready;
    const Waiting_receiver  ownrecv{&ready, recvbufp};

    // Enqueue our receive and wait for a sender to remove it.
    waitqp->push(ownrecv);
    ready.wait(*lockp, [&]{ return !waitqp->is_found(ownrecv); });
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
    task.promise().select(receive);
    return true;
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
    task.promise().select(send);
    return true;
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
    return task.promise().selected();
}


inline bool
Channel_select_awaitable::await_suspend(Task::Handle h)
{
    task = h;
    task.promise().select(first, last);
    return true;
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
    Non-Blocking Channel Operation Selection
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
    : selfp{fp}
{
}


template<class T>
inline bool
Future<T>::Awaitable::await_ready()
{
    return selfp->is_ready();
}


template<class T>
inline T
Future<T>::Awaitable::await_resume()
{
    /*
        If the future is ready we can get the result from one of its
        channels.  Otherwise, we have just been awakened after a call
        to select() having obtained either a value or an error.
    */
    if (selfp->is_ready())
        v = selfp->get_ready();
    else if (ep)
        rethrow_exception(ep);

    return std::move(v);
}


template<class T>
inline bool
Future<T>::Awaitable::await_suspend(Task::Handle task)
{
    ops[0] = selfp->vchan.make_receive(&v);
    ops[1] = selfp->echan.make_receive(&ep);
    task.promise().select(ops);
    return true;
}


/*
    Future     
*/
template<class T>
inline
Future<T>::Future()
    : isready{false}
{
}


template<class T>
inline
Future<T>::Future(Value_receiver vc, Error_receiver ec)
    : vchan{std::move(vc)}
    , echan{std::move(ec)}
    , isready{vchan.size() > 0 || echan.size() > 0}
{
}


template<class T>
inline
Future<T>::Future(Future&& other)
    : vchan{std::move(other.vchan)}
    , echan{std::move(other.echan)}
    , isready{other.isready}
{
}


template<class T>
inline Channel_base*
Future<T>::error_channel() const
{
    return echan.pimpl.get();
}


template<class T>
inline typename Future<T>::Awaitable
Future<T>::get()
{
    return this;
}


template<class T>
inline T
Future<T>::get_ready()
{
    optional<T> v = vchan.try_receive();

    if (v) {
        isready = false;
    } else if (optional<exception_ptr> ep = echan.try_receive()) {
        isready = false;
        rethrow_exception(*ep);
    }

    return *v;
}


template<class T>
inline bool
Future<T>::is_ready() const
{
    return isready;
}


template<class T>
inline bool
Future<T>::is_valid() const
{
    return vchan && echan ? true : false;
}


template<class T>
inline Future<T>&
Future<T>::operator=(Future&& other)
{
    swap(*this, other);
    return *this;
}


template<class T>
inline bool*
Future<T>::ready_flag() const
{
    return &isready;
}


template<class T>
optional<T>
Future<T>::try_get()
{
    optional<T> v = vchan.try_receive();

    if (v) {
        isready = false;
    } else if (optional<exception_ptr> ep = echan.try_receive()) {
        isready = false;
        rethrow_exception(*ep);
    }

    return v;
}


template<class T>
inline Channel_base*
Future<T>::value_channel() const
{
    return vchan.pimpl.get();
}


/*
    Future All Awaitable
*/
template<class T>
inline
Future_all_awaitable<T>::Future_all_awaitable(const Vector* vp)
    : first{&(*vp)[0]}
    , last{first + vp->size()}
{
}


template<class T>
inline
Future_all_awaitable<T>::Future_all_awaitable(const Future<T>* begin, const Future<T>* end)
    : first{begin}
    , last{end}
{
}


template<class T>
inline bool
Future_all_awaitable<T>::await_ready()
{
    return false;
}


template<class T>
inline void
Future_all_awaitable<T>::await_resume()
{
}


template<class T>
inline bool
Future_all_awaitable<T>::await_suspend(Task::Handle h)
{
    task = h;
    task.promise().wait_all(first, last);
    return true;
}


template<class T>
inline Future_all_awaitable<T>
wait_all(const std::vector<Future<T>>& fs)
{
    return Future_all_awaitable<T>(&fs);
}


/*
    Future Any Awaitable
*/
template<class T>
inline
Future_any_awaitable<T>::Future_any_awaitable(const Vector* vp)
    : first{&(*vp)[0]}
    , last{first + vp->size()}
{
}


template<class T>
inline
Future_any_awaitable<T>::Future_any_awaitable(const Future<T>* begin, const Future<T>* end)
    : first{begin}
    , last{end}
{
}


template<class T>
inline bool
Future_any_awaitable<T>::await_ready()
{
    return false;
}


template<class T>
inline typename std::vector<Future<T>>::size_type
Future_any_awaitable<T>::await_resume()
{
    return task.promise().ready_future();
}


template<class T>
inline bool
Future_any_awaitable<T>::await_suspend(Task::Handle h)
{
    task = h;
    task.promise().wait_any(first, last);
    return true;
}


template<class T>
inline Future_any_awaitable<T>
wait_any(const std::vector<Future<T>>& fs)
{
    return Future_any_awaitable<T>(&fs);
}


/*
    Task Launcher
*/
template<class TaskFun, class... Args>
inline void
start(TaskFun f, Args&&... args)
{
    using std::forward;

    scheduler.submit(f(forward<Args>(args)...));
}


/*
    Asynchronous Function Invocation
*/
template<class Fun, class... Args>
Future<std::result_of_t<Fun&&(Args&&...)>>
async(Fun f, Args&&... args)
{
    using std::current_exception;
    using std::forward;
    using std::move;
    using Result            = std::result_of_t<Fun&&(Args&&...)>;
    using Result_sender     = Send_channel<Result>;
    using Error_sender      = Send_channel<exception_ptr>;

    Channel<Result>         r       = make_channel<Result>(1);
    Channel<exception_ptr>  e       = make_channel<exception_ptr>(1);
    auto                    taskfun = [](Result_sender r, Error_sender e, Fun f, Args&&... args) -> Task {
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

