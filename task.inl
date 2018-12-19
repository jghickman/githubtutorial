//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/coroutine/task.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:55:18 $
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
    Task Future Selection Channel Wait
*/
inline
Task::Future_selection::Channel_wait::Channel_wait(Channel_base* channelp, Channel_size futpos)
    : chanp{channelp}
    , fpos{futpos}
{
}


inline Channel_base*
Task::Future_selection::Channel_wait::channel() const
{
    return chanp;
}


inline bool
Task::Future_selection::Channel_wait::dequeue(Task::Handle task, Channel_size pos) const
{
    const Channel_lock lock(chanp);
    return chanp->dequeue_readable_wait(task, pos);
}


inline void
Task::Future_selection::Channel_wait::dequeue_locked(Task::Handle task, Channel_size pos) const
{
    chanp->dequeue_readable_wait(task, pos);
}


inline void
Task::Future_selection::Channel_wait::enqueue(Task::Handle task, Channel_size pos) const
{
    chanp->enqueue_readable_wait(task, pos);
}


inline Channel_size
Task::Future_selection::Channel_wait::future() const
{
    return fpos;
}


inline bool
Task::Future_selection::Channel_wait::is_ready() const
{
    return chanp->is_readable();
}


inline void
Task::Future_selection::Channel_wait::lock_channel() const
{
    chanp->lock();
}


inline void
Task::Future_selection::Channel_wait::unlock_channel() const
{
    chanp->unlock();
}


/*
    Task Future Selection Future Wait
*/
inline
Task::Future_selection::Future_wait::Future_wait(bool* readyp, Channel_size vpos, Channel_size epos)
    : vchan{vpos}
    , echan{epos}
    , isreadyp{readyp}
{
}


inline void
Task::Future_selection::Future_wait::complete(Task::Handle task, const Channel_wait_vector& chans, Channel_size pos) const
{
    const Channel_size other = (pos == vchan) ? echan : vchan;

    chans[other].dequeue(task, other);
    *isreadyp = true;
}


inline bool
Task::Future_selection::Future_wait::dequeue(Task::Handle task, const Channel_wait_vector& chans) const
{
    bool is_dequeued = true;

    if (!chans[vchan].dequeue(task, vchan))
        is_dequeued = false;

    if (!chans[echan].dequeue(task, echan))
        is_dequeued = false;

    return is_dequeued;
}


inline void
Task::Future_selection::Future_wait::dequeue_locked(Task::Handle task, const Channel_wait_vector& chans) const
{
    chans[vchan].dequeue_locked(task, vchan);
    chans[echan].dequeue_locked(task, echan);
}


inline Channel_size
Task::Future_selection::Future_wait::error() const
{
    return echan;
}


inline void
Task::Future_selection::Future_wait::enqueue(Task::Handle task, const Channel_wait_vector& chans) const
{
    chans[vchan].enqueue(task, vchan);
    chans[echan].enqueue(task, echan);
}


inline bool
Task::Future_selection::Future_wait::is_ready(const Channel_wait_vector& chans) const
{
    if (!*isreadyp && (chans[vchan].is_ready() || chans[echan].is_ready()))
        *isreadyp = true;

    return *isreadyp;
}


inline bool
Task::Future_selection::Future_wait::operator==(const Future_wait& other) const
{
    return this->isreadyp == other.isreadyp;
}


inline bool
Task::Future_selection::Future_wait::operator< (const Future_wait& other) const
{
    if (this->isreadyp < other.isreadyp) return true;
    if (other.isreadyp < this->isreadyp) return false;
    if (this->vchan < other.vchan) return true;
    if (other.vchan < this->vchan) return false;
    if (this->echan < other.echan) return true;
    return false;
}


inline Channel_size
Task::Future_selection::Future_wait::value() const
{
    return vchan;
}


/*
    Task Future Selection Wait Setup
*/
template<class T>
inline
Task::Future_selection::Wait_setup::Wait_setup(const Future<T>* first, const Future<T>* last, Wait_set* wp)
    : pwaits(wp)
{
    pwaits->begin_setup(first, last);
}


inline
Task::Future_selection::Wait_setup::~Wait_setup()
{
    pwaits->end_setup();
}


/*
    Task Future Selection Wait Set
*/
template<class T>
void
Task::Future_selection::Wait_set::begin_setup(const Future<T>* first, const Future<T>* last)
{
    transform(first, last, &futures, &channels);
    index_unique(futures, &index);
    nenqueued = 0;
    locks.acquire(channels);
}


inline Channel_size
Task::Future_selection::Wait_set::enqueued() const
{
    return nenqueued;
}


template<class T>
void
Task::Future_selection::Wait_set::transform(const Future<T>* first, const Future<T>* last, Future_wait_vector* fwaitsp, Channel_wait_vector* cwaitsp)
{
    Future_wait_vector&     fwaits      = *fwaitsp;
    Channel_wait_vector&    cwaits      = *cwaitsp;
    const Channel_size      nfutures    = last - first;

    fwaits.resize(nfutures);
    cwaits.resize(nfutures * 2);

    for (const Future<T>* futurep = first; futurep != last; ++futurep) {
        const Channel_size fpos = futurep - first;
        const Channel_size vpos = fpos * 2;
        const Channel_size epos = vpos + 1;

        // TODO: Cheaper to modify an existing wait than copy a new one.
        cwaits[vpos] = Channel_wait{futurep->value(), fpos};
        cwaits[epos] = Channel_wait{futurep->error(), fpos};
        fwaits[fpos] = Future_wait{futurep->ready(), vpos, epos};
    }
}


/*
    Task Future Selection Timer
*/
inline void
Task::Future_selection::Timer::clear() const
{
    state = inactive;
}


inline void
Task::Future_selection::Timer::start(Task::Handle task, nanoseconds duration) const
{
    scheduler.start_wait_timer(task, duration);
    state = running;
}


/*
    Task Future Selection
*/
inline Channel_size
Task::Future_selection::selected() const
{
    return *result;
}


template<class T>
bool
Task::Future_selection::wait_all(Handle task, const Future<T>* first, const Future<T>* last, const optional<nanoseconds>& maxtime)
{
    const Wait_setup setup{first, last, &waits};

    waittype = Wait_type::all;
    timer.clear();
    result.reset();

    if (waits.enqueue_not_ready(task) == 0)
        result = wait_success;
    else if (maxtime) {
        if (*maxtime > 0ns)
            timer.start(task, *maxtime);
        else {
            waits.dequeue_all(task);
            result = wait_fail;
        }
    }

    return !waits.enqueued();
}


template<class T>
bool
Task::Future_selection::wait_any(Handle task, const Future<T>* first, const Future<T>* last, const optional<nanoseconds>& maxtime)
{
    const Wait_setup setup{first, last, &waits};

    waittype = Wait_type::any;
    timer.clear();

    result = waits.select_ready();
    if (!result) {
        if (!maxtime) {
            waits.enqueue_all(task);
        } else if (*maxtime > 0ns) {
            waits.enqueue_all(task);
            timer.start(task, *maxtime);
        } else {
            result = wait_fail;
        }
    }

    return !waits.enqueued();
}


/*
    Task Promise
*/
inline bool
Task::Promise::cancel_wait_timer()
{
    const Lock lock{mutex};
    return futures.cancel_timer();
}


inline bool
Task::Promise::complete_wait_timer(Time_point time)
{
    const Handle    task{Handle::from_promise(*this)};
    const Lock      lock{mutex};

    return futures.complete_timer(task, time);
}


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


template<Channel_size N>
inline void
Task::Promise::select(const Channel_operation (&ops)[N])
{
    select(begin(ops), end(ops));
}


inline void
Task::Promise::select(const Channel_operation* first, const Channel_operation* last)
{
    const Handle    task{Handle::from_promise(*this)};
    Lock            lock{mutex};

    if (!operations.select(task, first, last))
        suspend(&lock);
}


inline Channel_size
Task::Promise::selected_future() const
{
    return futures.selected();
}


inline Task::Select_status
Task::Promise::select_operation(Channel_size pos)
{
    const Handle    task{Handle::from_promise(*this)};
    const Lock      lock{mutex};

    return operations.select(task, pos);
}


inline Channel_size
Task::Promise::selected_operation() const
{
    return operations.selected();
}


inline Task::Select_status
Task::Promise::select_readable(Channel_size pos)
{
    const Handle    task{Handle::from_promise(*this)};
    const Lock      lock{mutex};

    return futures.select_readable(task, pos);
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
Task::Promise::wait_all(const Future<T>* first, const Future<T>* last, const optional<nanoseconds>& maxtime)
{
    const Handle    task{Handle::from_promise(*this)};
    Lock            lock{mutex};

    if (!futures.wait_all(task, first, last, maxtime))
        suspend(&lock);
}


template<class T>
void
Task::Promise::wait_any(const Future<T>* first, const Future<T>* last, const optional<nanoseconds>& maxtime)
{
    const Handle    task{Handle::from_promise(*this)};
    Lock            lock{mutex};

    if (!futures.wait_any(task, first, last, maxtime))
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
    Channel Readable Waiter
*/
template<class T>
inline
Channel<T>::Readable_waiter::Readable_waiter(Task::Handle task, Channel_size chan)
    : taskh{task}
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
Channel<T>::Readable_waiter::notify(Mutex* mtxp) const
{
    /*
        If the waiting task has already been awakened, it could be in the
        midst of dequeing itself from this channel.  To avoid a deadly
        embrace, the current thread unlocks the channel before notifying
        the task that it has become readable.
    */
    mtxp->unlock();
    select(taskh, chanpos);
    mtxp->lock();
}


template<class T>
void
Channel<T>::Readable_waiter::select(Task::Handle task, Channel_size chan)
{
    const Task::Select_status selection = task.promise().select_readable(chan);

    if (selection.is_complete())
        scheduler.resume(task);
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
Channel<T>::Buffer::enqueue(const Readable_waiter& r)
{
    readers.push_back(r);
}


template<class T>
inline bool
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
    return size() == max_size();
}


template<class T>
inline Channel_size
Channel<T>::Buffer::max_size() const
{
    return sizemax;
}


template<class T>
template<class U>
inline bool
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
Channel<T>::Buffer::push(U&& value, Mutex* mtxp)
{
    using std::move;

    const bool is_pushed = push_silent(move(value));

    if (is_pushed && !readers.empty()) {
        const Readable_waiter waiter = readers.front();
        readers.pop_front();
        waiter.notify(mtxp);
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
Channel<T>::Io_queue<U>::find(Task::Handle task, Channel_size pos)
{
    using std::find_if;

    return find_if(waiters.begin(), waiters.end(), waiter_eq(task, pos));
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
Channel<T>::Receiver::Receiver(Task::Handle tsk, Channel_size pos, T* valuep)
    : taskh{tsk}
    , oper{pos}
    , valp{valuep}
    , readyp{nullptr}
{
    assert(tsk);
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
Channel<T>::Receiver::dequeue(U* sendbufp, Mutex* mtxp) const
{
    using std::move;

    bool is_dequeued = true;

    if (taskh) {
        /*
            If the receiving task already been awakened, it could be in the
            midst of dequeing itself from this channel.  To avoid a deadly
            embrace, the current thread unlocks the channel before notifying
            the receiver that the operation can be completed.
        */
        mtxp->unlock();
        if (!select(taskh, oper, valp, sendbufp))
            is_dequeued = false;
        mtxp->lock();
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
Channel<T>::Receiver::select(Task::Handle task, Channel_size pos, T* valp, U* sendbufp)
{
    using std::move;

    const Task::Select_status   selection   = task.promise().select_operation(pos);
    bool                        is_selected = false;

    if (selection.position() == pos) {
        *valp = move(*sendbufp);
        is_selected = true;
    }

    if (selection.is_complete())
        scheduler.resume(task);

    return is_selected;
}


template<class T>
inline Task::Handle
Channel<T>::Receiver::task() const
{
    return taskh;
}


/*
    Channel Sender
*/
template<class T>
inline
Channel<T>::Sender::Sender(Task::Handle tsk, Channel_size pos, const T* rvaluep)
    : taskh{tsk}
    , oper{pos}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , readyp{nullptr}
{
    assert(tsk);
    assert(rvaluep);
}


template<class T>
inline
Channel<T>::Sender::Sender(Task::Handle tsk, Channel_size pos, T* lvaluep)
    : taskh{tsk}
    , oper{pos}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , readyp{nullptr}
{
    assert(tsk);
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
Channel<T>::Sender::dequeue(U* recvbufp, Mutex* mtxp) const
{
    bool is_dequeued = true;

    if (taskh) {
        /*
            If the sending task already been awakened, it could be in the
            midst of dequeing itself from this channel.  To avoid a deadly
            embrace, the current thread unlocks the channel before notifying
            the sender that the operation can be completed.
        */
        mtxp->unlock();
        if (!select(taskh, oper, lvalp, rvalp, recvbufp))
            is_dequeued = false;
        mtxp->lock();
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
Channel<T>::Sender::select(Task::Handle task, Channel_size pos, T* lvalp, const T* rvalp, U* recvbufp)
{
    const Task::Select_status   select      = task.promise().select_operation(pos);
    bool                        is_selected = false;

    if (select.position() == pos) {
        move(lvalp, rvalp, recvbufp);
        is_selected = true;
    }

    if (select.is_complete())
        scheduler.resume(task);

    return is_selected;
}


template<class T>
inline Task::Handle
Channel<T>::Sender::task() const
{
    return taskh;
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
Channel<T>::Impl::Impl(Channel_size bufsize)
    : buffer{bufsize}
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
Channel<T>::Impl::dequeue(U* waitqp, Task::Handle task, Channel_size pos)
{
    const auto wp       = waitqp->find(task, pos);
    const bool is_found = wp != waitqp->end();

    if (is_found)
        waitqp->erase(wp);
    
    return is_found;
}


template<class T>
bool
Channel<T>::Impl::dequeue_read(Task::Handle task, Channel_size pos)
{
    return dequeue(&receivers, task, pos);
}


template<class T>
bool
Channel<T>::Impl::dequeue_readable_wait(Task::Handle task, Channel_size pos)
{
    return buffer.dequeue(Readable_waiter(task, pos));
}


template<class T>
bool
Channel<T>::Impl::dequeue_write(Task::Handle task, Channel_size pos)
{
    return dequeue(&senders, task, pos);
}


template<class T>
void
Channel<T>::Impl::enqueue_read(Task::Handle task, Channel_size pos, void* valuep)
{
    enqueue_read(task, pos, static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::enqueue_read(Task::Handle task, Channel_size pos, T* valuep)
{
    const Receiver r{task, pos, valuep};
    receivers.push(r);
}


template<class T>
void
Channel<T>::Impl::enqueue_readable_wait(Task::Handle task, Channel_size pos)
{
    buffer.enqueue(Readable_waiter(task, pos));
}


template<class T>
void
Channel<T>::Impl::enqueue_write(Task::Handle task, Channel_size pos, const void* rvaluep)
{
    enqueue_write(task, pos, static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::enqueue_write(Task::Handle task, Channel_size pos, void* lvaluep)
{
    enqueue_write(task, pos, static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::enqueue_write(Task::Handle task, Channel_size pos, U* valuep)
{
    const Sender s{task, pos, valuep};
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
    return !(buffer.is_empty() && senders.is_empty());
}


template<class T>
bool
Channel<T>::Impl::is_writable() const
{
    return !(receivers.is_empty() && buffer.is_full());
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
Channel<T>::Impl::read(U* valuep, Buffer* bufp, Sender_queue* sendqp, Mutex* mtxp)
{
    return bufp->pop(valuep) || dequeue(sendqp, valuep, mtxp);
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
Channel<T>::Impl::write(U* valuep, Buffer* bufp, Receiver_queue* recvqp, Mutex* mtxp)
{
    using std::move;
    return dequeue(recvqp, valuep, mtxp) || bufp->push(move(*valuep), mtxp);
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
    return task.promise().selected_operation();
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
select(const Channel_operation (&ops)[N])
{
    return Channel_select_awaitable(begin(ops), end(ops));
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
    return try_select(begin(ops), end(ops));
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
Future<T>::error() const
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
Future<T>::ready() const
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
Future<T>::value() const
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
inline bool
All_futures_awaitable<T>::await_ready()
{
    return false;
}


template<class T>
inline void
All_futures_awaitable<T>::await_resume()
{
}


template<class T>
inline bool
All_futures_awaitable<T>::await_suspend(Task::Handle task)
{
    task.promise().wait_all(first, last, time);
    return true;
}


template<class T>
inline All_futures_awaitable<T>
wait_all(const Future<T>* first, const Future<T>* last)
{
    return All_futures_awaitable<T>(first, last);
}


template<class T>
inline All_futures_awaitable<T>
wait_all(const vector<Future<T>>& fs)
{
    const Future<T>* first{nullptr};
    const Future<T>* last{nullptr};

    if (!fs.empty()) {
        first = fs.data();
        last = first + fs.size();
    }

    return wait_all(first, last);
}


/*
    Future All Timed Awaitable
*/
template<class T>
inline
All_futures_timed_awaitable<T>::All_futures_timed_awaitable(const Future<T>* begin, const Future<T>* end, nanoseconds maxtime)
    : first{begin}
    , last{end}
    , time{maxtime}
{
}


template<class T>
inline bool
All_futures_timed_awaitable<T>::await_ready()
{
    return false;
}


template<class T>
inline bool
All_futures_timed_awaitable<T>::await_resume()
{
    return task.promise().selected_future() != wait_fail;
}


template<class T>
inline bool
All_futures_timed_awaitable<T>::await_suspend(Task::Handle taskh)
{
    task = taskh;
    task.promise().wait_all(first, last, time);
    return true;
}


template<class T>
inline All_futures_timed_awaitable<T>
wait_all(const Future<T>* first, const Future<T>* last, nanoseconds maxtime)
{
    return All_futures_timed_awaitable<T>(first, last, maxtime);
}


template<class T>
inline All_futures_timed_awaitable<T>
wait_all(const vector<Future<T>>& fs, nanoseconds maxtime)
{
    const Future<T>* first{nullptr};
    const Future<T>* last{nullptr};

    if (!fs.empty()) {
        first = fs.data();
        last = first + fs.size();
    }

    return wait_all(first, last, maxtime);
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
Any_future_awaitable<T>::Any_future_awaitable(const Future<T>* begin, const Future<T>* end, nanoseconds maxtime)
    : first{begin}
    , last{end}
    , time{maxtime}
{
}


template<class T>
inline bool
Any_future_awaitable<T>::await_ready()
{
    return false;
}


template<class T>
inline Channel_size
Any_future_awaitable<T>::await_resume()
{
    return task.promise().selected_future();
}


template<class T>
inline bool
Any_future_awaitable<T>::await_suspend(Task::Handle taskh)
{
    task = taskh;
    task.promise().wait_any(first, last, time);
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
wait_any(const Future<T>* first, const Future<T>* last, nanoseconds maxtime)
{
    return Any_future_awaitable<T>(first, last, maxtime);
}


template<class T>
inline Any_future_awaitable<T>
wait_any(const vector<Future<T>>& fs)
{
    const Future<T>* first{nullptr};
    const Future<T>* last{nullptr};

    if (!fs.empty()) {
        first = fs.data();
        last = first + fs.size();
    }

    return wait_any(first, last);
}


template<class T>
inline Any_future_awaitable<T>
wait_any(const vector<Future<T>>& fs, nanoseconds maxtime)
{
    const Future<T>* first{nullptr};
    const Future<T>* last{nullptr};

    if (!fs.empty()) {
        first = fs.data();
        last = first + fs.size();
    }

    return wait_any(first, last, maxtime);
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
Future<std::result_of_t<Fun(Args&&...)>>
async(Fun f, Args&&... args)
{
    using std::current_exception;
    using std::forward;
    using std::move;
    using Result            = std::result_of_t<Fun(Args&&...)>;
    using Result_sender     = Send_channel<Result>;
    using Error_sender      = Send_channel<exception_ptr>;

    const auto r    = make_channel<Result>(1);
    const auto e    = make_channel<exception_ptr>(1);
    const auto task = [](Result_sender r, Error_sender e, Fun f, Args&&... args) -> Task
    {
        exception_ptr ep;

        try {
            Result x = f(forward<Args>(args)...);
            co_await r.send(x);
        } catch (...) {
            ep = current_exception();
        }

        if (ep) co_await e.send(ep);
    };

    start(move(task), r, e, move(f), forward<Args>(args)...);
    return Future<Result>{r, e};
}


}  // Coroutine
}  // Isptech

//  $CUSTOM_FOOTER$
