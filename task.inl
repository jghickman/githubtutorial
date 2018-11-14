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
    Task Selection Status
*/
inline
Task::Selection_status::Selection_status()
{
}


inline
Task::Selection_status::Selection_status(Channel_size pos, bool comp)
    : selpos{pos}
    , iscomp{cmop}
{
}


inline bool
Task::Selection_status::is_complete() const
{
    return iscomp;
}


inline Channel_size
Task::Selection_status::position() const
{
    return selpos;
}


/*
    Task Future Selection Channel Wait
*/
inline
Task::Future_selection::Channel_wait::Channel_wait()
{
}


inline
Task::Future_selection::Channel_wait::Channel_wait(Channel_base* chanp, bool* rdyflagp, Channel_size fpos)
    : channelp{chanp}
    , readyp{rdyflagp}
    , futpos{fpos}
{
}


inline Channel_base*
Task::Future_selection::Channel_wait::channel() const
{
    return channelp;
}


inline void
Task::Future_selection::Channel_wait::complete() const
{
    *readyp = true;
}


inline void
Task::Future_selection::Channel_wait::dequeue(Task::Handle task, Channel_size pos) const
{
    channelp->dequeue_readable_wait(task, pos);
}


inline void
Task::Future_selection::Channel_wait::enqueue(Task::Handle task, Channel_size pos) const
{
    channelp->enqueue_readable_wait(task, pos);
}


inline Channel_size
Task::Future_selection::Channel_wait::future() const
{
    return futpos;
}


inline bool
Task::Future_selection::Channel_wait::is_ready() const
{
    return channelp->is_readable();
}


inline void
Task::Future_selection::Channel_wait::lock_channel() const
{
    channelp->lock();
}


inline void
Task::Future_selection::Channel_wait::unlock_channel() const
{
    channelp->unlock();
}


/*
    Task Future Selection Transform
*/
template<class T>
Task::Future_selection::Transform<T>::Transform(const Future<T>* first, const Future<T>* last, Channel_wait_vector* waitsp)
{
    waitsp->resize(2 * (last - first));
    transform(first, last, waitsp->begin());
    sort_channels(waitsp);
}


template<class T>
inline Task::Future_selection::Channel_wait
Task::Future_selection::Transform<T>::error_wait(const Future<T>* fp, Channel_size pos)
{
    return Channel_wait(fp->error_channel(), fp->ready_flag(), pos);
}


template<class T>
inline void
Task::Future_selection::Transform<T>::sort_channels(Channel_wait_vector* waitsp)
{
    std::sort(waitsp->begin(), waitsp->end(), [](const Channel_wait& x, const Channel_wait& y) {
        return x.channel() < y.channel();
    });
}


template<class T>
void
Task::Future_selection::Transform<T>::transform(const Future<T>* first, const Future<T>* last, Channel_wait_vector::iterator out)
{
    for (auto fp = first; fp != last; ++fp) {
        const auto fpos = fp - first;
        *out++ = value_wait(fp, fpos);
        *out++ = error_wait(fp, fpos);
    }
}


template<class T>
inline Task::Future_selection::Channel_wait
Task::Future_selection::Transform<T>::value_wait(const Future<T>* fp, Channel_size pos)
{
    return Channel_wait(fp->value_channel(), fp->ready_flag(), pos);
}


/*
    Task Future Selection
*/
inline Channel_size
Task::Future_selection::future_count(const Channel_wait_vector& ws)
{
    return ws.size() / 2;
}


template<class T>
bool
Task::Future_selection::wait_all(const Future<T>* first, const Future<T>* last, Handle task)
{
    Transform<T>    transform{first, last, &waits};
    Channel_locks   lock{&waits};
    Future_sort     sort{&waits};

    nenqueued

    return enqueue_not_ready(waits, task) == 0;
}


template<class T>
bool
Task::Future_selection::wait_any(const Future<T>* first, const Future<T>* last, Handle task)
{
    Transform<T>    transform(first, last, &waits);
    Channel_locks   lock(&waits);
    Future_sort     sort{&waits};

    nenqueued   = 0;
    ndequeued   = 0;
    chosen      = select_ready(waits);

    /*
        If nothing is ready, enqueue each wait on its corrsponding channel
        and keep them in lock order until we awake.
    */
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


inline bool
Task::Promise::notify_ready(Channel_size pos)
{
    const Lock lock{mutex};

    return futures.notify_ready(pos);
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
Task::Promise::selected() const
{
    return channels.selected();
}


inline void
Task::Promise::suspend(Lock* lockp)
{
    taskstat = Status::suspended;
    lockp->release();
}


template<Channel_size N>
inline optional<Channel_size>
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


inline Task&
Task::operator=(Task&& other)
{
    swap(*this, other);
    return *this;
}


inline Task::Status
Task::resume()
{
    Promise& promise = coro.promise();

    promise.make_ready();
    coro.resume();
    return promise.status();
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
    Channel Readable Channel_wait
*/
template<class T>
inline
Channel<T>::Readable_wait::Readable_wait()
{
}


template<class T>
inline
Channel<T>::Readable_wait::Readable_wait(Task::Handle task, Channel_size pos)
    : taskh{task}
    , waitpos{pos}
{
}


template<class T>
inline void
Channel<T>::Readable_wait::notify(Mutex* mtxp) const
{
    mtxp->unlock();
    if (taskh.promise().notify_ready(waitpos))
        scheduler.resume(taskh);
    mtxp->lock();
}


template<class T>
inline Channel_size
Channel<T>::Readable_wait::position() const
{
    return waitpos;
}


template<class T>
inline Task::Handle
Channel<T>::Readable_wait::task() const
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
Channel<T>::Buffer::enqueue(const Readable_wait& w)
{
    readers.push_back(w);
}


template<class T>
inline void
Channel<T>::Buffer::dequeue(const Readable_wait& w)
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
Channel<T>::Buffer::push(U&& value, Mutex* mtxp)
{
    push_silent(std::move(value));

    if (!readers.empty()) {
        const Readable_wait reader = readers.front();
        readers.pop_front();
        reader.notify(mtxp);
    }
}


template<class T>
template<class U>
void
Channel<T>::Buffer::push_silent(U&& value)
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
    Channel I/O Queue
*/
template<class T>
template<class U>
inline typename Channel<T>::Io_queue<U>::Iterator
Channel<T>::Io_queue<U>::end()
{
    return ws.end();
}


template<class T>
template<class U>
inline void
Channel<T>::Io_queue<U>::erase(Iterator p)
{
    ws.erase(p);
}


template<class T>
template<class U>
inline typename Channel<T>::Io_queue<U>::Iterator
Channel<T>::Io_queue<U>::find(Task::Handle task, Channel_size selpos)
{
    return std::find_if(ws.begin(), ws.end(), operation_eq(task, selpos));
}


template<class T>
template<class U>
inline bool
Channel<T>::Io_queue<U>::is_found(const Waiter& w) const
{
    using std::find;

    const auto p = find(ws.begin(), ws.end(), w);
    return p != ws.end();
}


template<class T>
template<class U>
inline bool
Channel<T>::Io_queue<U>::is_empty() const
{
    return ws.empty();
}


template<class T>
template<class U>
inline U
Channel<T>::Io_queue<U>::pop()
{
    const U w = ws.front();

    ws.pop_front();
    return w;
}


template<class T>
template<class U>
inline void
Channel<T>::Io_queue<U>::push(const Waiter& w)
{
    ws.push_back(w);
}


/*
    Channel Receiver
*/
template<class T>
inline
Channel<T>::Receiver::Receiver(Task::Handle tsk, Channel_size cop, T* valuep)
    : taskh{tsk}
    , chanop{cop}
    , valp{valuep}
    , threadcondp{nullptr}
{
    assert(tsk);
    assert(valuep);
}


template<class T>
inline
Channel<T>::Receiver::Receiver(Condition_variable* waiterp, T* valuep)
    : valp{valuep}
    , threadcondp{waiterp}
{
    assert(waiterp);
    assert(valuep);
}


template<class T>
template<class U>
bool
Channel<T>::Receiver::dequeue(U* valuep, Mutex* mtxp) const
{
    using std::move;

    bool is_selected = true;

    if (taskh) {
        /*
            The receiving task could be in the midst of trying to dequeue
            itself from this channel, so we release our own lock to avoid
            a deadly embrace.  Our work is done if we succeed in dequeueing
            the task; otherwise, we re-acquire our own lock to continue
            searching for a candidate receiver.
        */
        mtxp->unlock();
        if (!select(taskh, chanop, valuep)) {
            is_selected = false;
            mtsp->lock();
        }
    } else {
        *valp = move(*valuep);
        threadcondp->notify_one();
    }

    return is_selected;
}


template<class T>
template<class U>
bool
Channel<T>::Receiver::select(Task::Handle task, Channel_size chanop, U* valuep)
{
    using std::move;

    const Task::Selection_status    select      = task.promise().select(chanop);
    bool                            is_selected = false;

    if (select.position() == chanop) {
        *valp = move(*valuep);
        is_selected = true;
    }

    if (select.is_complete())
        scheduler.resume(task);

    return is_selected;
}


/*
    Channel Sender
*/
template<class T>
inline
Channel<T>::Sender::Sender(Task::Handle tsk, Channel_size cop, const T* rvaluep)
    : taskh{tsk}
    , chanop{cop}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , threadcondp{nullptr}
{
    assert(tsk);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Sender::Sender(Task::Handle tsk, Channel_size cop, T* lvaluep)
    : taskh{tsk}
    , chanpos{cop}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , threadcondp{nullptr}
{
    assert(tsk);
    assert(lvaluep);
}


template<class T>
inline
Channel<T>::Sender::Sender(Condition_variable* waiterp, const T* rvaluep)
    : rvalp{rvaluep}
    , lvalp{nullptr}
    , threadcondp{waiterp}
{
    assert(waiterp);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Sender::Sender(Condition_variable* waiterp, T* lvaluep)
    : rvalp{nullptr}
    , lvalp{lvaluep}
    , threadcondp{waiterp}
{
    assert(waiterp);
    assert(lvaluep);
}


template<class T>
template<class U>
bool
Channel<T>::Sender::dequeue(U* recvbufp, Mutex* mtxp) const
{
    bool is_sent = false;

    if (taskh) {
        /*
            The sending task could be in the midst of dequeueing itself
            from this channel, so we release our lock to avoid a deadly
            embrace.  If we succeed in dequeueing the task, we're done;
            otherwise, we must re-acquire the lock so the caller can safely
            search for another receiver.
        */
        mtxp->unlock();
        if (taskh.promise().select(pos))
            mtxp->lock();
        else {
            move(lvalp, rvalp, recvbufp);
            scheduler.resume(taskh);
            is_sent = true;
        }
    } else {
        move(lvalp, rvalp, recvbufp);
        threadcondp->notify_one();
        is_sent = true;
    }

    return is_sent;
}


template<class T>
template<class U>
bool
Channel<T>::Sender::dequeue(U* recvbufp, Mutex* mtxp) const
{
    bool is_selected = true;

    if (taskh) {
        /*
            The sending task could be in the midst of trying to dequeue
            itself from this channel, so we release our own lock to avoid
            a deadly embrace.  Our work is done if we succeed in dequeueing
            the task; otherwise, we re-acquire our own lock to continue
            searching for a candidate sender.
        */
        mtxp->unlock();
        if (!select(taskh, chanop, lvalp, rvalp, recvbufp)) {
            is_selected = false;
            mtsp->lock();
        }
    } else {
        move(lvalp, rvalp, recvbufp);
        threadcondp->notify_one();
    }

    return is_selected;
}


template<class T>
template<class U>
inline void
Channel<T>::Sender::move(T* lvalp, const T* rvalp, U* recvbufp)
{
    *recvbufp = lvalp ? std::move(*lvalp) : *rvalp;
}


template<class T>
inline void
Channel<T>::Sender::move(T* lvalp, const T* rvalp, Buffer* bufp)
{
    if (lvalp)
        bufp->push_silent(std::move(*lvalp));
    else
        bufp->push_silent(*rvalp);
}


template<class T>
template<class U>
bool
Channel<T>::Sender::select(Task::Handle task, Channel_size chanop, T* lvalp, const T* rvalp, U* recvbufp)
{
    const Task::Selection_status    select      = task.promise().select(chanop);
    bool                            is_selected = false;

    if (select.position() == chanop) {
        move(lvalp, rvalp, recvbufp);
        is_selected = true;
    }

    if (select.is_complete())
        scheduler.resume(task);

    return is_selected;
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

    if (!dequeue(&senders, &value, &mutex)) {
        if (!buffer.is_empty())
            buffer.pop(&value);
        else
            wait_for_sender(&receivers, &value, &lock);
    }

    return value;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::blocking_send(U* valuep)
{
    Lock lock{mutex};

    if (!dequeue(&receivers, valuep, &mutex)) {
        if (!buffer.is_full())
            buffer.push(move(*valuep), &mutex);
        else
            wait_for_receiver(&senders, valuep, &lock);
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
Channel<T>::Impl::dequeue(Receiver_queue* qp, U* sendbufp, Mutex* mtxp)
{
    bool is_sent = false;

    while (!(is_sent || qp->is_empty())) {
        const Receiver receiver = qp->pop();
        is_sent = receiver.dequeue(sendbufp, mtxp);
    }

    return is_sent;
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue(Sender_queue* qp, U* recvbufp, Mutex* mtxp)
{
    bool is_received = false;

    while (!(is_received || qp->is_empty())) {
        const Sender sender = qp->pop();
        is_received = sender.dequeue(recvbufp, mtxp);
    }

    return is_received;
}


template<class T>
template<class U>
inline bool
Channel<T>::Impl::dequeue(U* waitqp, Task::Handle task, Channel_size chanop)
{
    auto wp             = waitqp->find(task, chanop);
    const bool is_found = wp != waitqp->end();

    if (is_found)
        waitqp->erase(wp);
    
    return is_found;
}


template<class T>
bool
Channel<T>::Impl::dequeue_read(Task::Handle task, Channel_size chanop)
{
    return dequeue(&receivers, task, chanop);
}


template<class T>
void
Channel<T>::Impl::dequeue_readable_wait(Task::Handle task, Channel_size pos)
{
    buffer.dequeue(Readable_wait(task, pos));
}


template<class T>
bool
Channel<T>::Impl::dequeue_write(Task::Handle task, Channel_size chanop)
{
    return dequeue(&senders, task, chanop);
}


template<class T>
void
Channel<T>::Impl::enqueue_read(Task::Handle task, Channel_size selpos, void* valuep)
{
    enqueue_read(task, selpos, static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::enqueue_read(Task::Handle task, Channel_size selpos, T* valuep)
{
    const Receiver r{task, selpos, valuep};
    receivers.push(r);
}


template<class T>
void
Channel<T>::Impl::enqueue_readable_wait(Task::Handle task, Channel_size pos)
{
    buffer.enqueue(Readable_wait(task, pos));
}


template<class T>
void
Channel<T>::Impl::enqueue_write(Task::Handle task, Channel_size selpos, const void* rvaluep)
{
    enqueue_write(task, selpos, static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::enqueue_write(Task::Handle task, Channel_size selpos, void* lvaluep)
{
    enqueue_write(task, selpos, static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::enqueue_write(Task::Handle task, Channel_size selpos, U* valuep)
{
    const Sender s{task, selpos, valuep};
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
Channel<T>::Impl::is_readable() const
{
    return !(buffer.is_empty() && receivers.is_empty());
}


template<class T>
bool
Channel<T>::Impl::is_writable() const
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
Channel<T>::Impl::read(void* valuep)
{
    read(static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::read(T* valuep)
{
    if (!dequeue(&senders, valuep, &mutex))
        buffer.pop(valuep);
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

    if (!dequeue(&senders, &value, &mutex)) {
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

    if (dequeue(&receivers, &value, &mutex))
        is_sent = true;
    else if (!buffer.is_full()) {
        buffer.push(value, &mutex);
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
Channel<T>::Impl::wait_for_receiver(Sender_queue* waitqp, U* sendbufp, Lock* lockp)
{
    Condition_variable  ready;
    const Sender        ownsend{&ready, sendbufp};

    // Enqueue our send and wait for a receiver to remove it.
    waitqp->push(ownsend);
    ready.wait(*lockp, [&]{ return !waitqp->is_found(ownsend); });
}


template<class T>
void
Channel<T>::Impl::wait_for_sender(Receiver_queue* waitqp, T* recvbufp, Lock* lockp)
{
    Condition_variable  ready;
    const Receiver      ownrecv{&ready, recvbufp};

    // Enqueue our receive and wait for a sender to remove it.
    waitqp->push(ownrecv);
    ready.wait(*lockp, [&]{ return !waitqp->is_found(ownrecv); });
}


template<class T>
void
Channel<T>::Impl::write(const void* rvaluep)
{
    write(static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::write(void* lvaluep)
{
    write(static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::write(U* valuep)
{
    if (!dequeue(&receivers, valuep, &mutex))
        buffer.push(move(*valuep), &mutex);
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
    using std::move;

    optional<T> v = vchan.try_receive();

    if (v) {
        isready = false;
    } else if (optional<exception_ptr> ep = echan.try_receive()) {
        isready = false;
        rethrow_exception(*ep);
    }

    return move(*v);
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
Future_all_awaitable<T>::await_suspend(Task::Handle task)
{
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


#if 0
/*
    Asynchronous Function Invocation
*/
template<class Fun, class... Args>
Future<std::result_of_t<Fun(Args&&...)>>
async(Fun f, Args&&... args)
{
    using std::current_exception;
    using std::forward;
    using std::move;
    using Result            = std::result_of_t<Fun(Args&&...)>;
    using Result_sender     = Send_channel<Result>;
    using Error_sender      = Send_channel<exception_ptr>;

    Channel<Result>         r       = make_channel<Result>(1);
    Channel<exception_ptr>  e       = make_channel<exception_ptr>(1);
    auto                    taskfun = [](Result_sender r, Error_sender e, Fun f, Args&&... args) -> Task {
    exception_ptr           ep;

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
#endif


template<class Fun>
Future<std::result_of_t<Fun()>>
async(Fun f)
{
    using std::current_exception;
    using std::forward;
    using std::move;
    using Result            = std::result_of_t<Fun()>;
    using Result_sender     = Send_channel<Result>;
    using Error_sender      = Send_channel<exception_ptr>;

    Channel<Result>         r       = make_channel<Result>(1);
    Channel<exception_ptr>  e       = make_channel<exception_ptr>(1);
    auto                    taskfun = [](Result_sender r, Error_sender e, Fun f) -> Task {
    exception_ptr           ep;

        try {
            Result tmp = f();
            co_await r.send(tmp);
        } catch (...) {
            ep = current_exception();
        }

        if (ep) co_await e.send(ep);
    };

    start(move(taskfun), r, e, move(f));
    return Future<Result>{r, e};
}


}  // Concurrency
}  // Isptech

