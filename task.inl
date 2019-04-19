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
Task::Select_status::Select_status(Channel_size oper, bool complet)
    : op{oper}
    , iscomp{complet}
{
}


inline bool
Task::Select_status::is_complete() const
{
    return iscomp;
}


inline Channel_size
Task::Select_status::operation() const
{
    return op;
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
    transform(first, last, &fwaits, &cwaits);
    index_unique(fwaits, &index);
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
    return fwaits.empty();
}


inline Channel_size
Task::Future_selector::Future_set::size() const
{
    return fwaits.size();
}


template<class T>
void
Task::Future_selector::Future_set::transform(const Future<T>* first, const Future<T>* last, Future_wait_vector* fwaitsp, Channel_wait_vector* cwaitsp)
{
    const auto nfs = last - first;

    fwaitsp->clear();
    fwaitsp->reserve(nfs);

    cwaitsp->clear();
    cwaitsp->reserve(nfs * 2);

    for (const Future<T>* fp = first; fp != last; ++fp) {
        if (fp->is_valid()) {
            const Channel_size fpos = fwaitsp->size();
            const Channel_size vpos = cwaitsp->size();
            const Channel_size epos = vpos + 1;

            cwaitsp->push_back({fp->value_channel(), fpos});
            cwaitsp->push_back({fp->error_channel(), fpos});
            fwaitsp->push_back({fp->ready_flag(), vpos, epos});
        }
    }
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
Task::Future_selector::select_all(Task::Promise* taskp, const Future<T>* first, const Future<T>* last, optional<Duration> maxtime)
{
    using std::literals::chrono_literals::operator""ns;

    const Wait_setup setup{first, last, &futures};

    result.reset();
    npending = futures.enqueue(taskp);
    if (npending == 0) {
        if (!futures.is_empty())
            result = futures.size(); // all futures are ready
    } else if (maxtime) {
        if (*maxtime > 0ns)
            timer.start(taskp, *maxtime);
        else {
            futures.dequeue(taskp);
            npending = 0;
        }
    }

    return npending == 0;
}


template<class T>
bool
Task::Future_selector::select_any(Task::Promise* taskp, const Future<T>* first, const Future<T>* last, optional<Duration> maxtime)
{
    using std::literals::chrono_literals::operator""ns;

    const Wait_setup setup{first, last, &futures};

    npending = 0;
    result = futures.select_ready();
    if (!result) {
        if (!maxtime)
            futures.enqueue(taskp);
        else if (*maxtime > 0ns && futures.enqueue(taskp))
            timer.start(taskp, *maxtime);

        if (futures.enqueued())
            npending = 1;
    }

    return npending == 0;
}


inline optional<Channel_size>
Task::Future_selector::selection() const
{
    return result;
}


/*
    Task Promise
*/
inline void
Task::Promise::erase_local(Local_key key)
{
    locals.erase(key);
}


inline Task::Final_suspend
Task::Promise::final_suspend()
{
    taskstate = State::done;
    return Final_suspend{};
}


inline void*
Task::Promise::find_local(Local_key key)
{
    return locals.find(key);
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
    /*
        Enter the waiting state and release ownership of the task lock.  The
        scheduler will unlock the task after it has been dispositioned.
    */
    taskstate = State::waiting;
    lockp->release();
}


inline optional<Channel_size>
Task::Promise::try_select(const Channel_operation* first, const Channel_operation* last)
{
    return try_select(first, last);
}


inline void
Task::Promise::update_local(Local_key key, Local_impl&& obj)
{
    locals.update(key, std::move(obj));
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
    Task Local Default Deleter
*/
template<typename T>
inline void
Task_local<T>::Default_deleter::operator()(void* p) const
{
    delete static_cast<T*>(p);
}


/*
    Task Local User Deleter
*/
template<typename T>
template<typename D>
inline
Task_local<T>::User_deleter<D>::User_deleter(D&& impl)
    : destroy{std::forward<D>(impl)}
{
}


template<typename T>
template<typename D>
inline void
Task_local<T>::User_deleter<D>::operator()(void* p) const
{
    destroy(p);
}


/*
    Task Local
*/
template<typename T>
inline
Task_local<T>::Task_local()
    : deleter{make_deleter()}
{
}


template<typename T>
template<typename D>
inline
Task_local<T>::Task_local(D dimpl)
    : deleter{make_deleter(std::move(dimpl))}
{
}


template<typename T>
inline T*
Task_local<T>::get(Task::Promise* taskp)
{
    return static_cast<T*>(taskp->find_local(this));
}


template<typename T>
inline Task::Local_deleter
Task_local<T>::make_deleter()
{
    return Default_deleter();
}


template<typename T>
template<typename D>
inline Task::Local_deleter
Task_local<T>::make_deleter(D&& impl)
{
    return User_deleter<D>(std::forward<D>(impl));
}


template<typename T>
inline T*
Task_local<T>::release(Task::Promise* taskp)
{
    return static_cast<T*>(taskp->release_local(this));
}


template<typename T>
inline void
Task_local<T>::reset(Task::Promise* taskp, T* p)
{
    if (p)
        taskp->update_local(this, Local_object{p, deleter});
    else
        taskp->erase_local(this);
}


/*
    Task Local Implementation
*/
inline
Task::Local_impl::Local_impl(void* objectp, Local_deleter d)
    : p{objectp}
    , destroy{d}
{
}


inline
Task::Local_impl::Local_impl(Local_impl&& other)
    : p{other.p}
    , destroy{std::move(other.destroy)}
{
    other.p = nullptr;
}


inline
Task::Local_impl::~Local_impl()
{
    if (p) destroy(p);
}


inline void*
Task::Local_impl::get() const
{
    return p;
}


inline Task::Local_impl&
Task::Local_impl::operator=(Local_impl&& other)
{
    Local_impl temp{std::move(other)};
    
    swap(*this, other);
    return *this;
}


inline void*
Task::Local_impl::release()
{
    void* objectp = p;

    p = nullptr;
    return objectp;
}


inline void
Task::Local_impl::reset(void* objectp)
{
    if (p) destroy(p);
    p = objectp;
}


inline void
swap(Task::Local_impl& x, Task::Local_impl& y)
{
    using std::swap;

    swap(x.p, y.p);
    swap(x.destroy, y.destroy);
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
    Channel Operation
*/
inline
Channel_operation::Channel_operation()
    : type{Type::none}
    , chanp{nullptr}
    , valp{nullptr}
    , constvalp{nullptr}
{
}


inline
Channel_operation::Channel_operation(Channel_base* cp, const void* cvaluep)
    : type{Type::send}
    , chanp{cp}
    , valp{nullptr}
    , constvalp{cvaluep}
{
    assert(cp != nullptr);
    assert(cvaluep != nullptr);
}


inline
Channel_operation::Channel_operation(Channel_base* cp, void* valuep, Type kind)
    : type{kind}
    , chanp{cp}
    , valp{valuep}
    , constvalp{nullptr}
{
    assert(cp != nullptr);
    assert(valuep != nullptr);
}


inline Channel_base*
Channel_operation::channel() const
{
    return chanp;
}


inline bool
Channel_operation::is_valid() const
{
    return chanp && (valp || constvalp) && type != Type::none;
}


inline bool
operator==(const Channel_operation& x, const Channel_operation& y)
{
    if (x.valp != y.valp) return false;
    if (x.constvalp != y.constvalp) return false;
    if (x.chanp != y.chanp) return false;
    if (x.type != y.type) return false;
    return true;
}


inline bool
operator< (const Channel_operation& x, const Channel_operation& y)
{
    if (x.chanp < y.chanp) return true;
    if (y.chanp < x.chanp) return false;
    if (x.type < y.type) return true;
    if (y.type < x.type) return false;
    if (x.valp < y.valp) return true;
    if (y.valp < x.valp) return false;
    if (x.constvalp < y.constvalp) return true;
    return false;
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
    Channel Receive Operation
*/
template<class T>
inline
Channel<T>::Receive::Receive(Task::Promise* tskp, Channel_size oper, T* valuep)
    : taskp{tskp}
    , taskoper{oper}
    , threadcondp{nullptr}
    , bufp{valuep}
{
    assert(tskp != nullptr);
    assert(valuep != nullptr);
}


template<class T>
inline
Channel<T>::Receive::Receive(Condition* condp, T* valuep)
    : taskp{nullptr}
    , taskoper{-1}
    , threadcondp{condp}
    , bufp{valuep}
{
    assert(condp != nullptr);
    assert(valuep != nullptr);
}


template<class T>
template<class U>
bool
Channel<T>::Receive::dequeue(U* sendbufp, Mutex* mutexp) const
{
    using std::move;

    bool is_dequeued = false;

    if (taskp) {
        const Task::Select_status selection = notify_complete(taskp, taskoper, mutexp);

        if (selection.operation() == taskoper) {
            *bufp = move(*sendbufp);
            is_dequeued = true;
        }

        if (selection.is_complete())
            scheduler.resume(taskp);
    } else {
        *bufp = move(*sendbufp);
        threadcondp->notify_one();
        is_dequeued = true;
    }

    return is_dequeued;
}


template<class T>
inline Channel_size
Channel<T>::Receive::operation() const
{
    return taskoper;
}


template<class T>
inline Task::Promise*
Channel<T>::Receive::task() const
{
    return taskp;
}


/*
    Channel Send Operation
*/
template<class T>
inline
Channel<T>::Send::Send(Task::Promise* tskp, Channel_size oper, const T* lvaluep)
    : taskp{tskp}
    , taskoper{oper}
    , threadcondp{nullptr}
    , lvbufp{lvaluep}
    , rvbufp{nullptr}
{
    assert(tskp != nullptr);
    assert(lvaluep != nullptr);
}


template<class T>
inline
Channel<T>::Send::Send(Task::Promise* tskp, Channel_size oper, T* rvaluep)
    : taskp{tskp}
    , taskoper{oper}
    , threadcondp{nullptr}
    , lvbufp{nullptr}
    , rvbufp{rvaluep}
{
    assert(tskp != nullptr);
    assert(rvaluep != nullptr);
}


template<class T>
inline
Channel<T>::Send::Send(Condition* condp, const T* lvaluep)
    : taskp{nullptr}
    , taskoper{-1}
    , threadcondp{condp}
    , lvbufp{lvaluep}
    , rvbufp{nullptr}
{
    assert(condp != nullptr);
    assert(lvaluep != nullptr);
}


template<class T>
inline
Channel<T>::Send::Send(Condition* condp, T* rvaluep)
    : taskp{nullptr}
    , taskoper{-1}
    , threadcondp{condp}
    , lvbufp{nullptr}
    , rvbufp{rvaluep}
{
    assert(condp != nullptr);
    assert(rvaluep != nullptr);
}


template<class T>
template<class U>
bool
Channel<T>::Send::dequeue(U* recvbufp, Mutex* mutexp) const
{
    bool is_dequeued = false;

    if (taskp) {
        const Task::Select_status selection = notify_complete(taskp, taskoper, mutexp);

        if (selection.operation() == taskoper) {
            move(lvbufp, rvbufp, recvbufp);
            is_dequeued = true;
        }

        if (selection.is_complete())
            scheduler.resume(taskp);
    } else {
        move(lvbufp, rvbufp, recvbufp);
        threadcondp->notify_one();
        is_dequeued = true;
    }

    return is_dequeued;
}


template<class T>
template<class U>
inline void
Channel<T>::Send::move(const T* lvsendbufp, T* rvsendbufp, U* recvbufp)
{
    using std::move;

    *recvbufp = rvsendbufp ? move(*rvsendbufp) : *lvsendbufp;
}


template<class T>
inline void
Channel<T>::Send::move(const T* lvsendbufp, T* rvsendbufp, Buffer* recvbufp)
{
    using std::move;

    if (rvsendbufp)
        recvbufp->push_silent(move(*rvsendbufp));
    else
        recvbufp->push_silent(*lvsendbufp);
}


template<class T>
inline Channel_size
Channel<T>::Send::operation() const
{
    return taskoper;
}


template<class T>
inline Task::Promise*
Channel<T>::Send::task() const
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

    if (!receive(&value, &buffer, &sendq, &mutex))
        wait_for_sender(&receiveq, &value, &lock);

    return value;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::blocking_send(U* valuep)
{
    Lock lock{mutex};

    if (!send(valuep, &buffer, &receiveq, &mutex))
        wait_for_receiver(&sendq, valuep, &lock);
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
Channel<T>::Impl::dequeue(Receive_queue* qp, U* sendbufp, Mutex* mutexp)
{
    bool is_sent = false;

    while (!(qp->is_empty() || is_sent)) {
        const Receive receive = qp->pop();
        is_sent = receive.dequeue(sendbufp, mutexp);
    }

    return is_sent;
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue(Send_queue* qp, U* recvbufp, Mutex* mutexp)
{
    bool is_received = false;

    while (!(qp->is_empty() || is_received)) {
        const Send send = qp->pop();
        is_received = send.dequeue(recvbufp, mutexp);
    }

    return is_received;
}


template<class T>
template<class U>
inline bool
Channel<T>::Impl::dequeue(U* qp, Task::Promise* taskp, Channel_size oper)
{
    const auto opp      = qp->find(taskp, oper);
    const bool is_found = opp != qp->end();

    if (is_found)
        qp->erase(opp);
    
    return is_found;
}


template<class T>
bool
Channel<T>::Impl::dequeue_readable_wait(Task::Promise* taskp, Channel_size wait)
{
    return buffer.dequeue({taskp, wait});
}


template<class T>
bool
Channel<T>::Impl::dequeue_receive(Task::Promise* taskp, Channel_size oper)
{
    return dequeue(&receiveq, taskp, oper);
}


template<class T>
bool
Channel<T>::Impl::dequeue_send(Task::Promise* taskp, Channel_size oper)
{
    return dequeue(&sendq, taskp, oper);
}


template<class T>
void
Channel<T>::Impl::enqueue_receive(Task::Promise* taskp, Channel_size oper, void* valuep)
{
    enqueue_receive(taskp, oper, static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::enqueue_receive(Task::Promise* taskp, Channel_size oper, T* valuep)
{
    receiveq.push({taskp, oper, valuep});
}


template<class T>
void
Channel<T>::Impl::enqueue_readable_wait(Task::Promise* taskp, Channel_size wait)
{
    buffer.enqueue({taskp, wait});
}


template<class T>
void
Channel<T>::Impl::enqueue_send(Task::Promise* taskp, Channel_size oper, const void* lvaluep)
{
    enqueue_send(taskp, oper, static_cast<const T*>(lvaluep));
}


template<class T>
void
Channel<T>::Impl::enqueue_send(Task::Promise* taskp, Channel_size oper, void* rvaluep)
{
    enqueue_send(taskp, oper, static_cast<T*>(rvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::enqueue_send(Task::Promise* taskp, Channel_size oper, U* valuep)
{
    sendq.push({taskp, oper, valuep});
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
    return !(buffer.is_empty() && sendq.is_empty());
}


template<class T>
bool
Channel<T>::Impl::is_writable() const
{
    return !(buffer.is_full() && receiveq.is_empty());
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
Channel<T>::Impl::receive(void* valuep)
{
    receive(static_cast<T*>(valuep), &buffer, &sendq, &mutex);
}


template<class T>
template<class U>
inline bool
Channel<T>::Impl::receive(U* valuep, Buffer* bufp, Send_queue* qp, Mutex* mutexp)
{
    return bufp->pop(valuep) || dequeue(qp, valuep, mutexp);
}


template<class T>
void
Channel<T>::Impl::send(const void* lvaluep)
{
    send(static_cast<const T*>(lvaluep), &buffer, &receiveq, &mutex);
}


template<class T>
void
Channel<T>::Impl::send(void* rvaluep)
{
    send(static_cast<T*>(rvaluep), &buffer, &receiveq, &mutex);
}


template<class T>
template<class U>
inline bool
Channel<T>::Impl::send(U* valuep, Buffer* bufp, Receive_queue* qp, Mutex* mutexp)
{
    using std::move;
    return dequeue(qp, valuep, mutexp) || bufp->push(move(*valuep), mutexp);
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

    receive(&value, &buffer, &sendq, &mutex);
    return value;
}


template<class T>
bool
Channel<T>::Impl::try_send(const T& value)
{
    Lock lock{mutex};
    return send(&value, &buffer, &receiveq, &mutex);
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
Channel<T>::Impl::wait_for_receiver(Send_queue* qp, U* sendbufp, Lock* lockp)
{
    Condition   ready;
    const Send  send{&ready, sendbufp};

    // Enqueue the send and wait for a receiver to dequeue it.
    qp->push(send);
    ready.wait(*lockp, [&]{ return !qp->is_found(send); });
}


template<class T>
void
Channel<T>::Impl::wait_for_sender(Receive_queue* qp, T* recvbufp, Lock* lockp)
{
    Condition       ready;
    const Receive   receive{&ready, recvbufp};

    // Enqueue the receive and wait for a sender to dequeue it.
    qp->push(receive);
    ready.wait(*lockp, [&]{ return !qp->is_found(receive); });
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
Channel<T>::notify_complete(Task::Promise* taskp, Channel_size oper, Mutex* mutexp)
{
    /*
        If the waiting task already been awakened, it could be in the
        midst of dequeing itself from this channel.  To avoid a deadly
        embrace, unlock the channel before notifying the task that the
        operation can be completed.
    */
    const Unlock_sentry unlock{mutexp};
    return taskp->notify_operation_complete(oper);
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
start(TaskFun task, Args&&... args)
{
    using std::forward;

    scheduler.submit(task(forward<Args>(args)...));
}


/*
    Asynchronous Function Invocation
*/
template<class Fun, class... Args>
Future<std::result_of_t<Fun(Args&&...)>>
async(Fun fun, Args&&... args)
{
    using std::current_exception;
    using std::forward;
    using std::move;
    using Result            = std::result_of_t<Fun(Args&&...)>;
    using Result_sender     = Send_channel<Result>;
    using Error_sender      = Send_channel<exception_ptr>;

    auto rchan  = make_channel<Result>(1);
    auto echan  = make_channel<exception_ptr>(1);
    auto task   = [](Result_sender r, Error_sender e, Fun f, Args&&... fargs) -> Task
    {
        exception_ptr ep;

        try {
            co_await r.send(f(foward<Args>(fargs)...));
        } catch (...) {
            ep = current_exception();
        }

        if (ep)
            co_await e.send(ep);
    };

    start(move(task), rchan, echan, move(fun), forward<Args>(args)...);
    return Future<Result>{rchan, echan};
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
