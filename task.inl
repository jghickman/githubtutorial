//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/coroutine/task.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.5 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2019/01/18 18:39:52 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/coroutine/task.inl,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


/*
    Information and Sensor Processing Technology Coroutine Library
*/
namespace Isptech   {
namespace Coroutine {


/*
    Task Channel Lock
*/
inline
Task::Channel_lock::Channel_lock(Channel_base* cp)
    : chanp{cp}
{
    chanp->lock();
}


inline
Task::Channel_lock::~Channel_lock()
{
    chanp->unlock();
}


/*
    Task Select Status
*/
inline
Task::Select_status::Select_status(Channel_size pos, bool complet)
    : selpos{pos}
    , iscomp{complet}
{
}


inline bool
Task::Select_status::is_complete() const
{
    return iscomp;
}


inline Channel_size
Task::Select_status::position() const
{
    return selpos;
}


/*
    Task Future Selector Channel Wait
*/
inline
Task::Future_selector::Channel_wait::Channel_wait(Channel_base* channelp, Channel_size future)
    : chanp{channelp}
    , fpos{future}
{
}


inline Channel_base*
Task::Future_selector::Channel_wait::channel() const
{
    return chanp;
}


inline void
Task::Future_selector::Channel_wait::complete() const
{
    is_enq = false;
}


inline void
Task::Future_selector::Channel_wait::dequeue(Task::Promise* taskp, Channel_size pos) const
{
    if (is_enq && chanp->dequeue_readable_wait(taskp, pos))
        is_enq = false;
}


inline void
Task::Future_selector::Channel_wait::enqueue(Task::Promise* taskp, Channel_size pos) const
{
    chanp->enqueue_readable_wait(taskp, pos);
    is_enq = true;
}


inline Channel_size
Task::Future_selector::Channel_wait::future() const
{
    return fpos;
}


inline bool
Task::Future_selector::Channel_wait::is_enqueued() const
{
    return is_enq;
}


inline bool
Task::Future_selector::Channel_wait::is_ready() const
{
    return chanp->is_readable();
}


/*
    Task Future Selector Future Wait

    TODO:  Change vchan/echan to vwait/ewait everywhere appropriate?
*/
inline
Task::Future_selector::Future_wait::Future_wait(bool* readyp, Channel_size vchan, Channel_size echan)
    : signalp{readyp}
    , vpos{vchan}
    , epos{echan}
{
}


inline Channel_size
Task::Future_selector::Future_wait::error() const
{
    return epos;
}


inline void
Task::Future_selector::Future_wait::enqueue(Task::Promise* taskp, const Channel_wait_vector& chans) const
{
    chans[vpos].enqueue(taskp, vpos);
    chans[epos].enqueue(taskp, epos);
}


inline bool
Task::Future_selector::Future_wait::is_ready(const Channel_wait_vector& chans) const
{
    if (!*signalp && (chans[vpos].is_ready() || chans[epos].is_ready()))
        *signalp = true;

    return *signalp;
}


inline bool
Task::Future_selector::Future_wait::operator==(const Future_wait& other) const
{
    return this->signalp == other.signalp;
}


inline bool
Task::Future_selector::Future_wait::operator< (const Future_wait& other) const
{
    if (this->signalp < other.signalp) return true;
    if (other.signalp < this->signalp) return false;
    if (this->vpos < other.vpos) return true;
    if (other.vpos < this->vpos) return false;
    if (this->epos < other.epos) return true;
    return false;
}


inline Channel_size
Task::Future_selector::Future_wait::value() const
{
    return vpos;
}


/*
    Task Future Selector Wait Setup
*/
template<class T>
inline
Task::Future_selector::Wait_setup::Wait_setup(const Future<T>* first, const Future<T>* last, Future_set* fsp)
    : futuresp(fsp)
{
    futuresp->assign(first, last);
    futuresp->lock_channels();
}


inline
Task::Future_selector::Wait_setup::~Wait_setup()
{
    futuresp->unlock_channels();
}


/*
    Task Future Selector Future Set
*/
template<class T>
void
Task::Future_selector::Future_set::assign(const Future<T>* first, const Future<T>* last)
{
    transform(first, last, &futures, &channels);
    index_unique(futures, &index);
    nenqueued = 0;
}


inline Channel_size
Task::Future_selector::Future_set::enqueued() const
{
    return nenqueued;
}


inline bool
Task::Future_selector::Future_set::is_empty() const
{
    return futures.empty();
}


template<class T>
void
Task::Future_selector::Future_set::transform(const Future<T>* first, const Future<T>* last, Future_wait_vector* fwsp, Channel_wait_vector* cwsp)
{
    auto&       fwaits  = *fwsp;
    auto&       cwaits  = *cwsp;
    const auto  nfs     = last - first;

    fwaits.resize(nfs);
    cwaits.resize(nfs * 2);

    for (const Future<T>* fp = first; fp != last; ++fp) {
        const auto fpos = fp - first;
        const auto vpos = fpos * 2;
        const auto epos = vpos + 1;

        cwaits[vpos] = {fp->value_channel(), fpos};
        cwaits[epos] = {fp->error_channel(), fpos};
        fwaits[fpos] = {fp->ready_flag(), vpos, epos};
    }
}


inline Channel_size
Task::Future_selector::Future_set::size() const
{
    return futures.size();
}


/*
    Task Future Selector Timer
*/
inline void
Task::Future_selector::Timer::start(Task::Promise* taskp, Duration duration) const
{
    scheduler.start_timer(taskp, duration);
    state = running;
}


/*
    Task Future Selector
*/
inline bool
Task::Future_selector::is_selected() const
{
    return result ? true : false;
}


template<class T>
bool
Task::Future_selector::select_all(Task::Promise* taskp, const Future<T>* first, const Future<T>* last, optional<Duration> maxtimep)
{
    using std::literals::chrono_literals::operator""ns;

    const Wait_setup setup{first, last, &futures};

    result.reset();
    quota = futures.enqueue(taskp);
    if (quota == 0) {
        if (!futures.is_empty())
            result = futures.size(); // all ready
    } else if (maxtimep) {
        if (*maxtimep > 0ns)
            timer.start(taskp, *maxtimep);
        else {
            futures.dequeue(taskp);
            quota = 0;
        }
    }

    return quota == 0;
}


template<class T>
bool
Task::Future_selector::select_any(Task::Promise* taskp, const Future<T>* first, const Future<T>* last, optional<Duration> maxtimep)
{
    using std::literals::chrono_literals::operator""ns;

    const Wait_setup setup{first, last, &futures};

    quota = 0;
    result = futures.select_ready();
    if (!result) {
        if (!maxtimep)
            futures.enqueue(taskp);
        else if (*maxtimep > 0ns && futures.enqueue(taskp))
            timer.start(taskp, *maxtimep);

        if (futures.enqueued())
            quota = 1;
    }

    return quota == 0;
}


inline optional<Channel_size>
Task::Future_selector::selection() const
{
    return result;
}


/*
    Task Promise
*/
inline Task::Final_suspend
Task::Promise::final_suspend()
{
    taskstate = State::done;
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
Task::Promise::notify_channel_readable(Channel_size pos)
{
    const Lock lock{mutex};
    return futures.notify_channel_readable(this, pos);
}


inline Task::Select_status
Task::Promise::notify_operation_complete(Channel_size pos)
{
    const Lock lock{mutex};
    return operations.notify_complete(this, pos);
}


inline bool
Task::Promise::notify_timer_canceled()
{
    const Lock lock{mutex};
    return futures.notify_timer_canceled();
}


inline bool
Task::Promise::notify_timer_expired(Time when)
{
    const Lock lock{mutex};
    return futures.notify_timer_expired(this, when);
}


template<Channel_size N>
inline void
Task::Promise::select(const Channel_operation (&ops)[N])
{
    using std::begin;
    using std::end;

    select(begin(ops), end(ops));
}


inline void
Task::Promise::select(const Channel_operation* first, const Channel_operation* last)
{
    Lock lock{mutex};

    if (!operations.select(this, first, last))
        suspend(&lock);
}


inline optional<Channel_size>
Task::Promise::selected_future() const
{
    return futures.selection();
}


inline bool
Task::Promise::selected_futures() const
{
    return futures.selection() ? true : false;
}


inline Channel_size
Task::Promise::selected_operation() const
{
    return operations.selected();
}


inline void
Task::Promise::suspend(Lock* lockp)
{
    taskstate = State::waiting;
    lockp->release();
}


inline optional<Channel_size>
Task::Promise::try_select(const Channel_operation* first, const Channel_operation* last)
{
    return try_select(first, last);
}


template<class T>
void
Task::Promise::wait_all(const Future<T>* first, const Future<T>* last, optional<Duration> maxtime)
{
    Lock lock{mutex};

    if (!futures.select_all(this, first, last, maxtime))
        suspend(&lock);
}


template<class T>
void
Task::Promise::wait_any(const Future<T>* first, const Future<T>* last, optional<Duration> maxtime)
{
    Lock lock{mutex};

    if (!futures.select_any(this, first, last, maxtime))
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


inline
Task::operator bool() const
{
    return coro ? true : false;
}


inline Task::State
Task::resume()
{
    Promise& promise = coro.promise();

    promise.make_ready();
    coro.resume();
    return promise.state();
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
    Channel Unlock Sentry
*/
template<class T>
inline
Channel<T>::Unlock_sentry::Unlock_sentry(Mutex* mp)
    : mutexp{mp}
{
    mutexp->unlock();
}


template<class T>
inline
Channel<T>::Unlock_sentry::~Unlock_sentry()
{
    mutexp->lock();
}


/*
    Channel Readable Waiter
*/
template<class T>
inline
Channel<T>::Readable_waiter::Readable_waiter(Task::Promise* tp, Channel_size chan)
    : taskp{tp}
    , chanpos{chan}
{
}


template<class T>
inline Channel_size
Channel<T>::Readable_waiter::channel() const
{
    return chanpos;
}


template<class T>
inline void
Channel<T>::Readable_waiter::notify(Mutex* mutexp) const
{
    if (notify_channel_readable(taskp, chanpos, mutexp))
        scheduler.resume(taskp);
}


template<class T>
bool
Channel<T>::Readable_waiter::notify_channel_readable(Task::Promise* taskp, Channel_size pos, Mutex* mutexp)
{
    /*
        If the waiting task has already been awakened, it could be in the
        midst of dequeing itself from this channel.  To avoid a deadly
        embrace, unlock the channel before notifying the task that it has
        become readable.
    */
    const Unlock_sentry unlock{mutexp};
    return taskp->notify_channel_readable(pos);
}


template<class T>
inline Task::Promise*
Channel<T>::Readable_waiter::task() const
{
    return taskp;
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
inline Channel_size
Channel<T>::Buffer::capacity() const
{
    return sizemax;
}


template<class T>
inline void
Channel<T>::Buffer::enqueue(const Readable_waiter& r)
{
    readers.push_back(r);
}


template<class T>
bool
Channel<T>::Buffer::dequeue(const Readable_waiter& r)
{
    using std::find;

    const auto rp       = find(readers.begin(), readers.end(), r);
    const bool is_found = rp != readers.end();

    if (is_found)
        readers.erase(rp);

    return is_found;
}


template<class T>
inline bool
Channel<T>::Buffer::is_empty() const
{
    return elemq.empty();
}


template<class T>
inline bool
Channel<T>::Buffer::is_full() const
{
    return size() == capacity();
}


template<class T>
template<class U>
bool
Channel<T>::Buffer::pop(U* valuep)
{
    using std::move;

    const bool is_data = !is_empty();

    if (is_data) {
        *valuep = move(elemq.front());
        elemq.pop();
    }

    return is_data;
}


template<class T>
template<class U>
bool
Channel<T>::Buffer::push(U&& value, Mutex* mutexp)
{
    using std::move;

    const bool is_pushed = push_silent(move(value));

    if (is_pushed && !readers.empty()) {
        const Readable_waiter waiter = readers.front();
        readers.pop_front();
        waiter.notify(mutexp);
    }

    return is_pushed;
}


template<class T>
template<class U>
bool
Channel<T>::Buffer::push_silent(U&& value)
{
    using std::move;

    const bool is_capacity = !is_full();

    if (is_capacity)
        elemq.push(move(value));

    return is_capacity;
}


template<class T>
inline Channel_size
Channel<T>::Buffer::size() const
{
    return static_cast<Channel_size>(elemq.size());
}


/*
    Channel I/O Queue
*/
template<class T>
template<class U>
inline typename Channel<T>::Io_queue<U>::Iterator
Channel<T>::Io_queue<U>::end()
{
    return waiters.end();
}


template<class T>
template<class U>
inline void
Channel<T>::Io_queue<U>::erase(Iterator p)
{
    waiters.erase(p);
}


template<class T>
template<class U>
inline typename Channel<T>::Io_queue<U>::Iterator
Channel<T>::Io_queue<U>::find(Task::Promise* taskp, Channel_size pos)
{
    using std::find_if;

    return find_if(waiters.begin(), waiters.end(), waiter_eq(taskp, pos));
}


template<class T>
template<class U>
inline bool
Channel<T>::Io_queue<U>::is_found(const Waiter& w) const
{
    using std::find;

    const auto p = find(waiters.begin(), waiters.end(), w);
    return p != waiters.end();
}


template<class T>
template<class U>
inline bool
Channel<T>::Io_queue<U>::is_empty() const
{
    return waiters.empty();
}


template<class T>
template<class U>
inline U
Channel<T>::Io_queue<U>::pop()
{
    const U w = waiters.front();

    waiters.pop_front();
    return w;
}


template<class T>
template<class U>
inline void
Channel<T>::Io_queue<U>::push(const Waiter& w)
{
    waiters.push_back(w);
}


/*
    Channel Receiver
*/
template<class T>
inline
Channel<T>::Receiver::Receiver(Task::Promise* tp, Channel_size pos, T* valuep)
    : taskp{tp}
    , oper{pos}
    , valp{valuep}
    , readyp{nullptr}
{
    assert(tp);
    assert(valuep);
}


template<class T>
inline
Channel<T>::Receiver::Receiver(Condition* waitp, T* valuep)
    : valp{valuep}
    , readyp{waitp}
{
    assert(waitp);
    assert(valuep);
}


template<class T>
template<class U>
bool
Channel<T>::Receiver::dequeue(U* sendbufp, Mutex* mutexp) const
{
    using std::move;

    bool is_dequeued = true;

    if (taskp) {
        if (!select(taskp, oper, valp, sendbufp, mutexp))
            is_dequeued = false;
    } else {
        *valp = move(*sendbufp);
        readyp->notify_one();
    }

    return is_dequeued;
}


template<class T>
inline Channel_size
Channel<T>::Receiver::operation() const
{
    return oper;
}


template<class T>
template<class U>
bool
Channel<T>::Receiver::select(Task::Promise* taskp, Channel_size pos, T* valp, U* sendbufp, Mutex* mutexp)
{
    using std::move;

    const Task::Select_status   selection   = notify_operation_complete(taskp, pos, mutexp);
    bool                        is_selected = false;

    if (selection.position() == pos) {
        *valp = move(*sendbufp);
        is_selected = true;
    }

    if (selection.is_complete())
        scheduler.resume(taskp);

    return is_selected;
}


template<class T>
inline Task::Promise*
Channel<T>::Receiver::task() const
{
    return taskp;
}


/*
    Channel Sender
*/
template<class T>
inline
Channel<T>::Sender::Sender(Task::Promise* tskp, Channel_size pos, const T* rvaluep)
    : taskp{tskp}
    , oper{pos}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , readyp{nullptr}
{
    assert(tskp);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Sender::Sender(Task::Promise* tskp, Channel_size pos, T* lvaluep)
    : taskp{tskp}
    , oper{pos}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , readyp{nullptr}
{
    assert(tskp);
    assert(lvaluep);
}


template<class T>
inline
Channel<T>::Sender::Sender(Condition* waitp, const T* rvaluep)
    : rvalp{rvaluep}
    , lvalp{nullptr}
    , readyp{waitp}
{
    assert(waitp);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Sender::Sender(Condition* waitp, T* lvaluep)
    : rvalp{nullptr}    
    , lvalp{lvaluep}
    , readyp{waitp}
{
    assert(waitp);
    assert(lvaluep);
}


template<class T>
template<class U>
bool
Channel<T>::Sender::dequeue(U* recvbufp, Mutex* mutexp) const
{
    bool is_dequeued = true;

    if (taskp) {
        if (!select(taskp, oper, lvalp, rvalp, recvbufp, mutexp))
            is_dequeued = false;
    } else {
        move(lvalp, rvalp, recvbufp);
        readyp->notify_one();
    }

    return is_dequeued;
}


template<class T>
template<class U>
inline void
Channel<T>::Sender::move(T* lvalp, const T* rvalp, U* bufp)
{
    using std::move;

    *bufp = lvalp ? move(*lvalp) : *rvalp;
}


template<class T>
inline void
Channel<T>::Sender::move(T* lvalp, const T* rvalp, Buffer* bufp)
{
    using std::move;

    if (lvalp)
        bufp->push_silent(move(*lvalp));
    else
        bufp->push_silent(*rvalp);
}


template<class T>
inline Channel_size
Channel<T>::Sender::operation() const
{
    return oper;
}


template<class T>
template<class U>
bool
Channel<T>::Sender::select(Task::Promise* taskp, Channel_size pos, T* lvalp, const T* rvalp, U* recvbufp, Mutex* mutexp)
{
    const Task::Select_status   select      = notify_operation_complete(taskp, pos, mutexp);
    bool                        is_selected = false;

    if (select.position() == pos) {
        move(lvalp, rvalp, recvbufp);
        is_selected = true;
    }

    if (select.is_complete())
        scheduler.resume(taskp);

    return is_selected;
}


template<class T>
inline Task::Promise*
Channel<T>::Sender::task() const
{
    return taskp;
}


/*
    Channel Receive Awaitable
*/
template<class T>
inline
Channel<T>::Receive_awaitable::Receive_awaitable(Impl* chanp)
    : receive{chanp->make_receive(&value)}
{
}


template<class T>
inline bool
Channel<T>::Receive_awaitable::await_ready()
{
    return false;
}


template<class T>
inline T
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
Channel<T>::Send_awaitable::Send_awaitable(Impl* chanp, U* valuep)
    : send{chanp->make_send(valuep)}
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
inline bool
Channel<T>::Send_awaitable::await_suspend(Task::Handle task)
{
    task.promise().select(send);
    return true;
}


/*
    Channel Implementation
*/
template<class T>
inline
Channel<T>::Impl::Impl(Channel_size bufsize)
    : buffer{bufsize}
{
}


template<class T>
inline typename Channel<T>::Receive_awaitable
Channel<T>::Impl::awaitable_receive()
{
    return Receive_awaitable(this);
}


template<class T>
template<class U>
inline typename Channel<T>::Send_awaitable
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

    if (!read(&value, &buffer, &senders, &mutex))
        wait_for_sender(&receivers, &value, &lock);

    return value;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::blocking_send(U* valuep)
{
    Lock lock{mutex};

    if (!write(valuep, &buffer, &receivers, &mutex))
        wait_for_receiver(&senders, valuep, &lock);
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
Channel<T>::Impl::dequeue(Receiver_queue* qp, U* sendbufp, Mutex* mutexp)
{
    bool is_sent = false;

    while (!(qp->is_empty() || is_sent)) {
        const Receiver receiver = qp->pop();
        is_sent = receiver.dequeue(sendbufp, mutexp);
    }

    return is_sent;
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue(Sender_queue* qp, U* recvbufp, Mutex* mutexp)
{
    bool is_received = false;

    while (!(qp->is_empty() || is_received)) {
        const Sender sender = qp->pop();
        is_received = sender.dequeue(recvbufp, mutexp);
    }

    return is_received;
}


template<class T>
template<class U>
inline bool
Channel<T>::Impl::dequeue(U* waitqp, Task::Promise* taskp, Channel_size pos)
{
    const auto wp       = waitqp->find(taskp, pos);
    const bool is_found = wp != waitqp->end();

    if (is_found)
        waitqp->erase(wp);
    
    return is_found;
}


template<class T>
bool
Channel<T>::Impl::dequeue_read(Task::Promise* taskp, Channel_size pos)
{
    return dequeue(&receivers, taskp, pos);
}


template<class T>
bool
Channel<T>::Impl::dequeue_readable_wait(Task::Promise* taskp, Channel_size pos)
{
    return buffer.dequeue(Readable_waiter(taskp, pos));
}


template<class T>
bool
Channel<T>::Impl::dequeue_write(Task::Promise* taskp, Channel_size pos)
{
    return dequeue(&senders, taskp, pos);
}


template<class T>
void
Channel<T>::Impl::enqueue_read(Task::Promise* taskp, Channel_size pos, void* valuep)
{
    enqueue_read(taskp, pos, static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::enqueue_read(Task::Promise* taskp, Channel_size pos, T* valuep)
{
    receivers.push({taskp, pos, valuep});
}


template<class T>
void
Channel<T>::Impl::enqueue_readable_wait(Task::Promise* taskp, Channel_size pos)
{
    buffer.enqueue({taskp, pos});
}


template<class T>
void
Channel<T>::Impl::enqueue_write(Task::Promise* taskp, Channel_size pos, const void* rvaluep)
{
    enqueue_write(taskp, pos, static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::enqueue_write(Task::Promise* taskp, Channel_size pos, void* lvaluep)
{
    enqueue_write(taskp, pos, static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::enqueue_write(Task::Promise* taskp, Channel_size pos, U* valuep)
{
    senders.push({taskp, pos, valuep});
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
Channel<T>::Impl::is_full() const
{
    const Lock lock{mutex};
    return buffer.is_full();
}


template<class T>
bool
Channel<T>::Impl::is_readable() const
{
    return !(buffer.is_empty() && senders.is_empty());
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
    read(static_cast<T*>(valuep), &buffer, &senders, &mutex);
}


template<class T>
template<class U>
inline bool
Channel<T>::Impl::read(U* valuep, Buffer* bufp, Sender_queue* sendqp, Mutex* mutexp)
{
    return bufp->pop(valuep) || dequeue(sendqp, valuep, mutexp);
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

    read(&value, &buffer, &senders, &mutex);
    return value;
}


template<class T>
bool
Channel<T>::Impl::try_send(const T& value)
{
    Lock lock{mutex};
    return write(&value, &buffer, &receivers, &mutex);
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
    Condition       ready;
    const Sender    ownsend{&ready, sendbufp};

    // Enqueue our send and wait for a receiver to remove it.
    waitqp->push(ownsend);
    ready.wait(*lockp, [&]{ return !waitqp->is_found(ownsend); });
}


template<class T>
void
Channel<T>::Impl::wait_for_sender(Receiver_queue* waitqp, T* recvbufp, Lock* lockp)
{
    Condition       ready;
    const Receiver  ownrecv{&ready, recvbufp};

    // Enqueue our receive and wait for a sender to remove it.
    waitqp->push(ownrecv);
    ready.wait(*lockp, [&]{ return !waitqp->is_found(ownrecv); });
}


template<class T>
void
Channel<T>::Impl::write(const void* rvaluep)
{
    write(static_cast<const T*>(rvaluep), &buffer, &receivers, &mutex);
}


template<class T>
void
Channel<T>::Impl::write(void* lvaluep)
{
    write(static_cast<T*>(lvaluep), &buffer, &receivers, &mutex);
}


template<class T>
template<class U>
inline bool
Channel<T>::Impl::write(U* valuep, Buffer* bufp, Receiver_queue* recvqp, Mutex* mutexp)
{
    using std::move;
    return dequeue(recvqp, valuep, mutexp) || bufp->push(move(*valuep), mutexp);
}


/*
    Channel
*/
template<class T>
inline
Channel<T>::Channel(Impl_ptr p)
    : pimpl{std::move(p)}
{
}


template<class T>
inline
Channel<T>::Channel(Channel&& other)
    : pimpl{std::move(other.pimpl)}
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
inline Task::Select_status
Channel<T>::notify_operation_complete(Task::Promise* taskp, Channel_size pos, Mutex* mutexp)
{
    /*
        If the waiting task already been awakened, it could be in the
        midst of dequeing itself from this channel.  To avoid a deadly
        embrace, unlock the channel before notifying the task that the
        operation can be completed.
    */
    const Unlock_sentry unlock{mutexp};
    return taskp->notify_operation_complete(pos);
}


template<class T>
inline Channel<T>&
Channel<T>::operator=(Channel&& other)
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
    return pimpl->awaitable_receive();
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
Channel<T>
make_channel(Channel_size capacity)
{
    return std::make_shared<Channel<T>::Impl>(capacity);
}


/*
    Receive Channel
*/
template<class T>
inline
Receive_channel<T>::Receive_channel(Channel<T> chan)
    : pimpl{std::move(chan.pimpl)}
{
}


template<class T>
inline Channel_size
Receive_channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline bool
Receive_channel<T>::is_empty() const
{
    return pimpl->is_empty();
}


template<class T>
inline bool
Receive_channel<T>::is_full() const
{
    return pimpl->is_full();
}


template<class T>
inline Channel_operation
Receive_channel<T>::make_receive(T* valuep) const
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
Receive_channel<T>::operator=(Channel<T> other)
{
    pimpl = std::move(other.pimpl);
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


/*
    Send Channel
*/
template<class T>
inline
Send_channel<T>::Send_channel(Channel<T> other)
    : pimpl{std::move(other.pimpl)}
{
}


template<class T>
inline Channel_size
Send_channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline bool
Send_channel<T>::is_empty() const
{
    return pimpl->is_empty();
}


template<class T>
inline bool
Send_channel<T>::is_full() const
{
    return pimpl->is_full();
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
Send_channel<T>::operator=(Channel<T> other)
{
    pimpl = std::move(other.pimpl);
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


/*
    Channel Select Awaitable
*/
inline
Channel_select_awaitable::Channel_select_awaitable(const Channel_operation* begin, const Channel_operation* end)
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
    return promisep->selected_operation();
}


inline bool
Channel_select_awaitable::await_suspend(Task::Handle task)
{
    promisep = &task.promise();
    promisep->select(first, last);
    return true;
}


/*
    Channel Select
*/
inline Channel_select_awaitable
select(const Channel_operation* first, const Channel_operation* last)
{
    return Channel_select_awaitable(first, last);
}


template<Channel_size N>
inline Channel_select_awaitable
select(const Channel_operation (&ops)[N])
{
    using std::begin;
    using std::end;

    return select(begin(ops), end(ops));
}


/*
    Non-Blocking Channel Operation Selection
*/
template<Channel_size N>
inline optional<Channel_size>
try_select(const Channel_operation* first, const Channel_operation* last)
{
    return Task::Promise::try_select(first, last);
}


template<Channel_size N>
inline optional<Channel_size>
try_select(const Channel_operation (&ops)[N])
{
    using std::begin;
    using std::end;

    return try_select(begin(ops), end(ops));
}


/*
    Channel of "void" Awaitable
*/
inline
Channel<void>::Awaitable::Awaitable(const Channel_operation& op)
    : operation{op}
{
}


inline bool
Channel<void>::Awaitable::await_ready()
{
    return false;
}


inline void
Channel<void>::Awaitable::await_resume()
{
}


inline bool
Channel<void>::Awaitable::await_suspend(Task::Handle task)
{
    task.promise().select(operation);
    return true;
}


/*
    Channel of "void" Implementation
*/
inline
Channel<void>::Impl::Impl(Channel_size bufsize)
    : Channel<char>::Impl(bufsize)
{
}


inline void
Channel<void>::Impl::blocking_receive()
{
    scratch = Channel<char>::Impl::blocking_receive();
}


inline void
Channel<void>::Impl::blocking_send()
{
    Channel<char>::Impl::blocking_send(&scratch);
}


inline Channel_operation
Channel<void>::Impl::make_receive()
{
    return Channel<char>::Impl::make_receive(&scratch);
}


inline Channel_operation
Channel<void>::Impl::make_send()
{
    return Channel<char>::Impl::make_send(&scratch);
}


inline Channel<void>::Awaitable
Channel<void>::Impl::receive()
{
    return Channel<char>::Impl::make_receive(&scratch);
}


inline Channel<void>::Awaitable
Channel<void>::Impl::send()
{
    return Channel<char>::Impl::make_send(&scratch);
}


inline bool
Channel<void>::Impl::try_receive()
{
    optional<char> cp = Channel<char>::Impl::try_receive();
    return cp ? true : false;
}


inline bool
Channel<void>::Impl::try_send()
{
    return Channel<char>::Impl::try_send(scratch);
}


/*
    Channel of "void"
*/
inline
Channel<void>::Channel(Impl_ptr p)
    : pimpl{std::move(p)}
{
}


inline
Channel<void>::Channel(Channel&& other)
    : pimpl{std::move(other.pimpl)}
{
}


inline Channel_size
Channel<void>::capacity() const
{
    return pimpl->capacity();
}


inline bool
Channel<void>::is_empty() const
{
    return pimpl->is_empty();
}


inline Channel_operation
Channel<void>::make_receive() const
{
    return pimpl->make_receive();
}


inline Channel_operation
Channel<void>::make_send() const
{
    return pimpl->make_send();
}


inline Channel<void>&
Channel<void>::operator=(Channel&& other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


inline
Channel<void>::operator bool() const
{
    return pimpl ? true : false;
}


inline Channel<void>::Receive_awaitable
Channel<void>::receive() const
{
    return pimpl->receive();
}


inline Channel<void>::Send_awaitable
Channel<void>::send() const
{
    return pimpl->send();
}


inline Channel_size
Channel<void>::size() const
{
    return pimpl->size();
}


inline bool
Channel<void>::try_receive() const
{
    return pimpl->try_receive();
}


inline bool
Channel<void>::try_send() const
{
    return pimpl->try_send();
}


inline void
blocking_receive(const Channel<void>& c)
{
    c.pimpl->blocking_receive();
}


inline void
blocking_send(const Channel<void>& c)
{
    c.pimpl->blocking_send();
}


inline bool
operator==(const Channel<void>& x, const Channel<void>& y)
{
    return x.pimpl == y.pimpl;
}


inline bool
operator< (const Channel<void>& x, const Channel<void>& y)
{
    return x.pimpl < y.pimpl;
}

    
inline void
swap(Channel<void>& x, Channel<void>& y)
{
    swap(x.pimpl, y.pimpl);
}


/*
    Receive Channel of "void"
*/
inline
Receive_channel<void>::Receive_channel(Channel<void> chan)
    : pimpl{std::move(chan.pimpl)}
{
}


inline Channel_size
Receive_channel<void>::capacity() const
{
    return pimpl->capacity();
}


inline bool
Receive_channel<void>::is_empty() const
{
    return pimpl->is_empty();
}


inline bool
Receive_channel<void>::is_full() const
{
    return pimpl->is_full();
}


inline Channel_operation
Receive_channel<void>::make_receive() const
{
    return pimpl->make_receive();
}


inline Receive_channel<void>&
Receive_channel<void>::operator=(Receive_channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


inline Receive_channel<void>&
Receive_channel<void>::operator=(Channel<void> other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


inline
Receive_channel<void>::operator bool() const
{
    return pimpl ? true : false;
}


inline Receive_channel<void>::Awaitable
Receive_channel<void>::receive() const
{
    return pimpl->receive();
}


inline Channel_size
Receive_channel<void>::size() const
{
    return pimpl->size();
}


inline bool
Receive_channel<void>::try_receive() const
{
    return pimpl->try_receive();
}


inline bool
operator==(const Receive_channel<void>& x, const Receive_channel<void>& y)
{
    return x.pimpl != y.pimpl;
}


inline bool
operator< (const Receive_channel<void>& x, const Receive_channel<void>& y)
{
    return x.pimpl < y.pimpl;
}


inline void
swap(Receive_channel<void>& x, Receive_channel<void>& y)
{
    swap(x.pimpl, y.pimpl);
}


/*
    Send Channel of "void"
*/
inline
Send_channel<void>::Send_channel(Channel<void> other)
    : pimpl{std::move(other.pimpl)}
{
}


inline Channel_size
Send_channel<void>::capacity() const
{
    return pimpl->capacity();
}


inline bool
Send_channel<void>::is_empty() const
{
    return pimpl->is_empty();
}


inline bool
Send_channel<void>::is_full() const
{
    return pimpl->is_full();
}


inline Channel_operation
Send_channel<void>::make_send() const
{
    return pimpl->make_send();
}


inline Send_channel<void>&
Send_channel<void>::operator=(Send_channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


inline Send_channel<void>&
Send_channel<void>::operator=(Channel<void> other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


inline
Send_channel<void>::operator bool() const
{
    return pimpl ? true : false;
}


inline Send_channel<void>::Awaitable
Send_channel<void>::send() const
{
    return pimpl->send();
}


inline Channel_size
Send_channel<void>::size() const
{
    return pimpl->size();
}


inline bool
Send_channel<void>::try_send() const
{
    return pimpl->try_send();
}


inline bool
operator==(const Send_channel<void>& x, const Send_channel<void>& y)
{
    return x.pimpl == y.pimpl;
}


inline bool
operator< (const Send_channel<void>& x, const Send_channel<void>& y)
{
    return x.pimpl < y.pimpl;
}


inline void
swap(Send_channel<void>& x, Send_channel<void>& y)
{
    swap(x.pimpl, y.pimpl);
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
T
Future<T>::Awaitable::await_resume()
{
    /*
        If a future is ready, the result can be obtained from one of its
        channels without waiting.  Otherwise, the task waiting on this
        future has just awakened from a call to select() having received
        either a value or an error.
    */
    if (selfp->is_ready())
        v = selfp->get_ready();
    else if (ep)
        rethrow_exception(ep);

    return std::move(v);
}


template<class T>
bool
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
T
Future<T>::get_ready()
{
    using std::move;

    optional<T> vp = vchan.try_receive();

    if (vp)
        isready = false;
    else if (optional<exception_ptr> epp = echan.try_receive()) {
        isready = false;
        rethrow_exception(*epp);
    }

    return move(*vp);
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
inline typename Future<T>::Awaitable
Future<T>::operator co_await()
{
    return get();
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
    optional<T> vp = vchan.try_receive();

    if (vp) {
        isready = false;
    } else if (optional<exception_ptr> epp = echan.try_receive()) {
        isready = false;
        rethrow_exception(*epp);
    }

    return vp;
}


template<class T>
inline Channel_base*
Future<T>::value_channel() const
{
    return vchan.pimpl.get();
}


/*
    Future "void" Awaitable
*/
inline
Future<void>::Awaitable::Awaitable(Future* fp)
    : selfp{fp}
{
}


inline bool
Future<void>::Awaitable::await_ready()
{
    return selfp->is_ready();
}


/*
    Future "void"
*/
inline
Future<void>::Future()
    : isready{false}
{
}


inline
Future<void>::Future(Value_receiver vc, Error_receiver ec)
    : vchan{std::move(vc)}
    , echan{std::move(ec)}
    , isready{vchan.size() > 0 || echan.size() > 0}
{
}


inline
Future<void>::Future(Future&& other)
    : vchan{std::move(other.vchan)}
    , echan{std::move(other.echan)}
    , isready{other.isready}
{
}


inline Channel_base*
Future<void>::error_channel() const
{
    return echan.pimpl.get();
}


inline Future<void>::Awaitable
Future<void>::get()
{
    return this;
}


inline bool
Future<void>::is_ready() const
{
    return isready;
}


inline bool
Future<void>::is_valid() const
{
    return vchan && echan ? true : false;
}


inline Future<void>&
Future<void>::operator=(Future&& other)
{
    swap(*this, other);
    return *this;
}


inline Future<void>::Awaitable
Future<void>::operator co_await()
{
    return get();
}


inline bool*
Future<void>::ready_flag() const
{
    return &isready;
}


inline Channel_base*
Future<void>::value_channel() const
{
    return vchan.pimpl.get();
}


/*
    All Futures Awaitable
*/
template<class T>
inline
All_futures_awaitable<T>::All_futures_awaitable(const Future<T>* begin, const Future<T>* end)
    : first{begin}
    , last{end}
{
}


template<class T>
inline
All_futures_awaitable<T>::All_futures_awaitable(const Future<T>* begin, const Future<T>* end, Duration maxtime)
    : first{begin}
    , last{end}
    , timeout{maxtime}
{
}


template<class T>
inline bool
All_futures_awaitable<T>::await_ready()
{
    return false;
}


template<class T>
inline bool
All_futures_awaitable<T>::await_resume()
{
    return task.promise().selected_futures();
}


template<class T>
inline bool
All_futures_awaitable<T>::await_suspend(Task::Handle taskh)
{
    task = taskh;
    task.promise().wait_all(first, last, timeout);
    return true;
}


template<class T>
inline All_futures_awaitable<T>
wait_all(const Future<T>* first, const Future<T>* last)
{
    return All_futures_awaitable<T>(first, last);
}


template<class T, Channel_size N>
inline All_futures_awaitable<T>
wait_all(const Future<T> (&fs)[N])
{
    return wait_all(fs, fs + N);
}


template<class T>
inline All_futures_awaitable<T>
wait_all(const std::vector<Future<T>>& fs)
{
    auto first = fs.data();
    return wait_all(first, first + fs.size());
}


template<class T>
inline All_futures_awaitable<T>
wait_all(const Future<T>* first, const Future<T>* last, Duration maxtime)
{
    return All_futures_awaitable<T>(first, last, maxtime);
}


template<class T, Channel_size N>
inline All_futures_awaitable<T>
wait_all(const Future<T> (&fs)[N], Duration maxtime)
{
    return wait_all(fs, fs + N, maxtime);
}


template<class T>
inline All_futures_awaitable<T>
wait_all(const std::vector<Future<T>>& fs, Duration maxtime)
{
    auto first = fs.data();
    return wait_all(first, first + fs.size(), maxtime);
}


/*
    Any Future Awaitable
*/
template<class T>
inline
Any_future_awaitable<T>::Any_future_awaitable(const Future<T>* begin, const Future<T>* end)
    : first{begin}
    , last{end}
{
}


template<class T>
inline
Any_future_awaitable<T>::Any_future_awaitable(const Future<T>* begin, const Future<T>* end, Duration maxtime)
    : first{begin}
    , last{end}
    , timeout{maxtime}
{
}


template<class T>
inline bool
Any_future_awaitable<T>::await_ready()
{
    return false;
}


template<class T>
inline optional<Channel_size>
Any_future_awaitable<T>::await_resume()
{
    return task.promise().selected_future();
}


template<class T>
inline bool
Any_future_awaitable<T>::await_suspend(Task::Handle taskh)
{
    task = taskh;
    task.promise().wait_any(first, last, timeout);
    return true;
}


template<class T>
inline Any_future_awaitable<T>
wait_any(const Future<T>* first, const Future<T>* last)
{
    return Any_future_awaitable<T>(first, last);
}


template<class T>
inline Any_future_awaitable<T>
wait_any(const Future<T>* first, const Future<T>* last, Duration maxtime)
{
    return Any_future_awaitable<T>(first, last, maxtime);
}


template<class T, Channel_size N>
inline Any_future_awaitable<T>
wait_any(const Future<T> (&fs)[N])
{
    return wait_any(fs, fs + N);
}


template<class T, Channel_size N>
inline Any_future_awaitable<T>
wait_any(const Future<T> (&fs)[N], Duration maxtime)
{
    return wait_any(fs, fs + N, maxtime);
}


template<class T>
inline Any_future_awaitable<T>
wait_any(const std::vector<Future<T>>& fs)
{
    auto first = fs.data();
    return wait_any(first, first + fs.size());
}


template<class T>
inline Any_future_awaitable<T>
wait_any(const std::vector<Future<T>>& fs, Duration maxtime)
{
    auto first = fs.data();
    return wait_any(first, first + fs.size(), maxtime);
}


/*
    Task Launcher
*/
template<class TaskFun, class... Args>
inline void
start(TaskFun taskfun, Args&&... args)
{
    using std::forward;

    scheduler.submit(taskfun(forward<Args>(args)...));
}


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

    auto r          = make_channel<Result>(1);
    auto e          = make_channel<exception_ptr>(1);
    auto taskfun    = [](Result_sender r, Error_sender e, Fun f, Args&&... args) -> Task
    {
        exception_ptr ep;

        try {
            Result x = f(forward<Args>(args)...);
            co_await r.send(move(x));
        } catch (...) {
            ep = current_exception();
        }

        if (ep)
            co_await e.send(ep);
    };

    start(move(taskfun), move(r), move(e), move(f), forward<Args>(args)...);
    return Future<Result>{r, e};
}


/*
    Timer
*/
inline
Timer::Timer(Timer&& other)
    : chan{std::move(other.chan)}
{
}


inline
Timer::~Timer()
{
    stop();
}


inline bool
Timer::is_active() const
{
    return chan && chan.is_empty();
}


inline bool
Timer::is_ready() const
{
    return chan && !chan.is_empty();
}


inline Channel_operation
Timer::make_receive(Time* tp)
{
    return chan.make_receive(tp);
}


inline Timer&
Timer::operator=(Timer&& other)
{
    chan = std::move(other.chan);
    return *this;
}


inline
Timer::operator bool() const
{
    return chan ? true : false;
}


inline Timer::Awaitable
Timer::receive()
{
    return chan.receive();
}


inline bool
Timer::stop()
{
    return chan ? scheduler.stop_timer(chan) : false;
}


inline optional<Time>
Timer::try_receive()
{
    return chan.try_receive();
}


inline Time
blocking_receive(Timer& timer)
{
    return blocking_receive(timer.chan);
}


inline bool
operator==(const Timer& x, const Timer& y)
{
    return x.chan == y.chan;
}


inline bool
operator< (const Timer& x, const Timer& y)
{
    return x.chan < y.chan;
}


inline void
swap(Timer& x, Timer& y)
{
    using std::swap;
    swap(x.chan, y.chan);
}


}  // Coroutine
}  // Isptech

//  $CUSTOM_FOOTER$
