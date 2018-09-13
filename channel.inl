//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/channel.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/channel.inl,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Goroutine
*/
inline
Goroutine::Goroutine(Handle h)
    : coro{h}
    , isowner{true}
{
}


inline
Goroutine::Goroutine(Goroutine&& other)
{
    steal(&other);
}


inline Goroutine&
Goroutine::operator=(Goroutine&& other)
{
    destroy(this);
    steal(&other);
    return *this;
}


inline
Goroutine::~Goroutine()
{
    destroy(this);
}


inline void
Goroutine::destroy(Goroutine* gp)
{
    if (gp->coro && gp->isowner)
        gp->coro.destroy();
}


inline Goroutine::Handle
Goroutine::handle() const
{
    return coro;
}


inline bool
Goroutine::is_owner() const
{
    return isowner;
}


inline
Goroutine::operator bool() const
{
    return coro ? true : false;
}


inline void
Goroutine::release()
{
    coro    = nullptr;
    isowner = false;
}


inline void
Goroutine::reset(Handle h)
{
    destroy(this);
    coro = h;
    isowner = true;
}


inline void
Goroutine::run()
{
    coro.resume();
    if (!coro.promise().is_done())
        release();
}


inline void
Goroutine::steal(Goroutine* otherp)
{
    coro    = otherp->coro;
    isowner = otherp->isowner;
    otherp->release();
}


inline bool
operator==(const Goroutine& x, const Goroutine& y)
{
    if (x.coro != y.coro) return false;
    if (x.isowner != y.isowner) return false;
    return true;
}


inline bool
Goroutine::Final_suspend::await_ready()
{
    return false;
}


inline void
Goroutine::Final_suspend::await_resume()
{
}


inline bool
Goroutine::Final_suspend::await_suspend(Goroutine::Handle coro)
{
    coro.promise().done();
    return true;
}


/*
    Goroutine Promise
*/
inline
Goroutine::Promise::Promise()
    : isdone{false}
{
}


inline void
Goroutine::Promise::done()
{
    isdone = true;
}


inline Goroutine::Final_suspend
Goroutine::Promise::final_suspend() const
{
    return Final_suspend{};
}


inline Goroutine
Goroutine::Promise::get_return_object()
{
    return Goroutine(Handle::from_promise(*this));
}


inline Goroutine::Initial_suspend
Goroutine::Promise::initial_suspend() const
{
    return Initial_suspend{};
}


inline bool
Goroutine::Promise::is_done() const
{
    return isdone;
}


inline void
Goroutine::Promise::return_void()
{
}


/*
    Channel
*/
template<class T>
inline
Channel<T>::Channel()
    : pimpl{std::move(p)}
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
    return pimpl;
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
    return pimpl->send(&value);
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
Channel<T>::Impl::dequeue_receive(Receive_queue* waitqp, U* sendbufp)
{
    bool is_dequeued = false;

    while (!(is_dequeued || waitqp->is_empty())) {
        const Waiting_receive receiver = waitqp->pop();
        is_dequeued = receiver.dequeue(sendbufp);
    }

    return is_dequeued;
}


template<class T>
template<class U>
bool
Channel<T>::Impl::dequeue_send(Send_queue* waitqp, U* recvbufp)
{
    bool is_dequeued = false;

    while (!(is_dequeued || waitqp->is_empty())) {
        const Waiting_send sender = waitqp->pop();
        is_dequeued = sender.dequeue(recvbufp);
    }

    return is_dequeued;
}


template<class T>
void
Channel<T>::Impl::enqueue_receive(const Detail::Channel_alternative::Impl_ptr& altp, Channel_size altpos, void* valuep)
{
    enqueue_receive(altp, altpos, static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::enqueue_receive(const Detail::Channel_alternative::Impl_ptr& altp, Channel_size altpos, T* valuep)
{
    const Waiting_receive r{altp, altpos, valuep};
    recvq.push(r);
}


template<class T>
void
Channel<T>::Impl::enqueue_send(const Detail::Channel_alternative::Impl_ptr& altp, Channel_size altpos, const void* rvaluep)
{
    enqueue_send(altp, altpos, static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::enqueue_send(const Detail::Channel_alternative::Impl_ptr& altp, Channel_size altpos, void* lvaluep)
{
    enqueue_send(altp, altpos, static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::enqueue_send(const Detail::Channel_alternative::Impl_ptr& altp, Channel_size altpos, U* valuep)
{
    const Waiting_send s{altp, altpos, valuep};
    sendq.push(s);
}


template<class T>
bool
Channel<T>::Impl::is_receive_ready() const
{
    return !(buffer.is_empty() && sendq.is_empty());
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
    return Channel_operation(this, Channel_operation::receive, valuep);
}


template<class T>
Channel_operation
Channel<T>::Impl::make_send(T* valuep)
{
    return Channel_operation(this, Channel_operation::send, valuep);
}


template<class T>
Channel_operation
Channel<T>::Impl::make_send(const T* valuep)
{
    return Channel_operation(this, valuep);
}


template<class T>
void
Channel<T>::Impl::pop(Buffer* bufp, T* recvbufp, Send_queue* waitqp)
{
    bufp->pop(recvbufp);
    dequeue_send(waitqp, bufp);
}


template<class T>
void
Channel<T>::Impl::ready_receive(void* valuep)
{
    ready_receive(static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::ready_receive(T* valuep)
{
    if (!buffer.is_empty())
        pop(&buffer, valuep, &sendq);
    else
        dequeue_send(&sendq, valuep);
}


template<class T>
void
Channel<T>::Impl::ready_send(const void* rvaluep)
{
    ready_send(static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::ready_send(void* lvaluep)
{
    ready_send(static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::ready_send(U* valuep)
{
    if (!dequeue_receive(&recvq, valuep))
        buffer.push(move(*valuep));
}


template<class T>
void
Channel<T>::Impl::send(const void* rvaluep)
{
    send(static_cast<const T*>(rvaluep));
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

    if (!buffer.is_empty())
        pop(&buffer, &value, &sendq);
    else if (!dequeue_send(&sendq, &value))
        wait_for_send(lock, &recvq, &value);

    return value;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::sync_send(U* valuep)
{
    Lock lock{mutex};

    if (!buffer.is_full())
        buffer.push(move(*valuep));
    else if (!dequeue_receive(&recvq, valuep))
        wait_for_receive(lock, &sendq, valuep);
}


template<class T>
optional<T>
Channel<T>::Impl::try_receive()
{
    optional<T> value;
    Lock        lock{mutex};

    if (!buffer.is_empty())
        pop(&buffer, &value, &sendq);
    else
        dequeue_send(&sendq, &value);

    return value;
}


template<class T>
bool
Channel<T>::Impl::try_send(const T& value)
{
    bool is_sent{true};
    Lock lock{mutex};

    if (!buffer.is_full())
        buffer.push(value);
    else if (!dequeue_receive(&recvq, &value))
        is_sent = false;

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
Channel<T>::Impl::wait_for_receive(Lock& lock, Send_queue* waitqp, U* sendbufp)
{
    condition_variable ready;
    const Waiting_send ownthread{&ready, sendbufp};

    waitqp->push(ownthread);
    ready.wait(lock, [&]{ return !waitqp->find(ownthread); });
}


template<class T>
void
Channel<T>::Impl::wait_for_send(Lock& lock, Receive_queue* waitqp, T* recvbufp)
{
    condition_variable      ready;
    const Waiting_receive   ownthread{&ready, recvbufp};

    waitqp->push(ownthread);
    ready.wait(lock, [&]{ return !waitqp->find(ownthread); });
}


/*
    Channel Receive Awaitable
*/
template<class T>
inline
Channel<T>::Receive_awaitable::Receive_awaitable(Impl* channelp)
    : ops{channelp->make_receive(&value)}
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
Channel<T>::Receive_awaitable::await_suspend(Goroutine::Handle g)
{
    return !alt.select(ops, g);
}


/*
    Channel Send Awaitable
*/
template<class T>
template<class U>
inline
Channel<T>::Send_awaitable::Send_awaitable(Impl* channelp, U* valuep)
    : ops{channelp->make_send(valuep)}
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
Channel<T>::Send_awaitable::await_suspend(Goroutine::Handle g)
{
    return !alt.select(ops, g);
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
    return pimpl;
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
    return pimpl;
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
    Implementation Details
*/
namespace Detail {


/*
    Names/Types
*/
using std::move;
    
    
/*
    Channel Buffer
*/
template<class T>
inline
Channel_buffer<T>::Channel_buffer(Channel_size maxsize)
    : sizemax{maxsize >= 0 ? maxsize : 0}
{
    assert(maxsize >= 0);
}


template<class T>
inline bool
Channel_buffer<T>::is_empty() const
{
    return q.empty();
}


template<class T>
inline bool
Channel_buffer<T>::is_full() const
{
    return size() == max_size();
}


template<class T>
inline Channel_size
Channel_buffer<T>::max_size() const
{
    return sizemax;
}


template<class T>
inline void
Channel_buffer<T>::pop(T* valuep)
{
    using std::move;

    *valuep = move(q.front());
    q.pop();
}


template<class T>
inline void
Channel_buffer<T>::pop(optional<T>* valuep)
{
    using std::move;

    *valuep = move(q.front());
    q.pop();
}


template<class T>
template<class U>
inline void
Channel_buffer<T>::push(U&& value)
{
    using std::move;

    q.push(move(value));
}


template<class T>
inline Channel_size
Channel_buffer<T>::size() const
{
    return static_cast<Channel_size>(q.size());
}


/*
    Wait Queue
*/
template<class T>
inline void
Wait_queue<T>::erase(Goroutine::Handle g)
{
    using std::find_if;

    const auto wp = find_if(ws.begin(), ws.end(), goroutine_eq(g));
    if (wp != ws.end())
        ws.erase(wp);
}


template<class T>
inline bool
Wait_queue<T>::is_empty() const
{
    return ws.empty();
}


template<class T>
inline bool
Wait_queue<T>::find(const Waiter& w) const
{
    using std::find;

    const auto p = find(ws.begin(), ws.end(), w);
    return p != ws.end();
}


template<class T>
inline T
Wait_queue<T>::pop()
{
    const T w = ws.front();

    ws.pop_front();
    return w;
}


template<class T>
inline void
Wait_queue<T>::push(const Waiter& w)
{
    ws.push_back(w);
}


/*
    Waiting Receiver
*/
template<class T>
inline
Waiting_receive<T>::Waiting_receive(const Channel_alternative::Impl_ptr& ap, Channel_size apos, T* valuep)
    : altp{ap}
    , altpos{apos}
    , valp{valuep}
    , sysrecvp{nullptr}
{
    assert(ap);
    assert(valuep);
}


template<class T>
inline
Waiting_receive<T>::Waiting_receive(condition_variable* sysreadyp, T* valuep)
    : valp{valuep}
    , sysrecvp{sysreadyp}
{
    assert(sysreadyp);
    assert(valuep);
}


template<class T>
template<class U>
inline bool
Waiting_receive<T>::dequeue(U* valuep) const
{
    bool is_dequeued = false;

    if (altp) {
        Channel_alternative::Lock altlock(altp->mutex);
        if (!altp->chosen) {
            const Goroutine::Handle g = altp->waiting;

            *valp = std::move(*valuep);
            altp->chosen = altpos;
            altlock.unlock();
            scheduler.resume(g);
            is_dequeued = true;
        }
    } else {
        *valp = std::move(*valuep);
        sysrecvp->notify_one();
        is_dequeued = true;
    }

    return is_dequeued;
}


template<class T>
inline condition_variable*
Waiting_receive<T>::system_signal() const
{
    return sysrecvp;
}


template<class T>
inline bool
operator==(const Waiting_receive<T>& x, const Waiting_receive<T>& y)
{
    if (x.sysrecvp!= y.sysrecvp) return false;
    if (x.valp != y.valp) return false;
    if (x.altp != y.altp) return false;
    if (x.altpos != y.altpos) return false;
    return true;
}


/*
    Waiting Sender
*/
template<class T>
inline
Waiting_send<T>::Waiting_send(const Channel_alternative::Impl_ptr& ap, Channel_size apos, const T* rvaluep)
    : altp{ap}
    , altpos{apos}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , syssenderp{nullptr}
{
    assert(ap);
    assert(rvaluep);
}


template<class T>
inline
Waiting_send<T>::Waiting_send(const Channel_alternative::Impl_ptr& ap, Channel_size apos, T* lvaluep)
    : altp{ap}
    , altpos{apos}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , syssenderp{nullptr}
{
    assert(ap);
    assert(lvaluep);
}


template<class T>
inline
Waiting_send<T>::Waiting_send(condition_variable* sysreadyp, const T* rvaluep)
    : altp{nullptr}
    , altpos{0}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , syssenderp{sysreadyp}
{
    assert(sysreadyp);
    assert(rvaluep);
}


template<class T>
inline
Waiting_send<T>::Waiting_send(condition_variable* sysreadyp, T* lvaluep)
    : altp{nullptr}
    , altpos{0}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , syssenderp{sysreadyp}
{
    assert(sysreadyp);
    assert(lvaluep);
}


template<class T>
template<class U>
bool
Waiting_send<T>::dequeue(U* recvbufp) const
{
    bool is_dequeued = false;

    if (altp) {
        Channel_alternative::Lock altlock(altp->mutex);

        if (!altp->chosen) {
            const Goroutine::Handle sender = altp->waiting;
            move(lvalp, rvalp, recvbufp);
            altp->chosen = altpos;
            altlock.unlock();
            scheduler.resume(sender);
            is_dequeued = true;
        }
    } else {
        move(lvalp, rvalp, recvbufp);
        syssenderp->notify_one();
        is_dequeued = true;
    }

    return is_dequeued;
}


template<class T>
template<class U>
inline void
Waiting_send<T>::move(T* lvalp, const T* rvalp, U* recvbufp)
{
    *recvbufp = lvalp ? std::move(*lvalp) : *rvalp;
}


template<class T>
inline void
Waiting_send<T>::move(T* lvalp, const T* rvalp, Channel_buffer<T>* bufp)
{
    if (lvalp)
        bufp->push(std::move(*lvalp));
    else
        bufp->push(*rvalp);
}


template<class T>
inline condition_variable*
Waiting_send<T>::system_signal() const
{
    return syssenderp;
}


template<class T>
inline bool
operator==(const Waiting_send<T>& x, const Waiting_send<T>& y)
{
    if (x.syssenderp != y.syssenderp) return false;
    if (x.rvalp != y.rvalp) return false;
    if (x.lvalp != y.lvalp) return false;
    if (x.altp != y.altp) return false;
    if (x.altpos != y.altpos) return false;
    return true;
}


/*
    Channel Alternative
*/
inline const Channel_alternative::Impl&
Channel_alternative::read() const
{
    return *pimpl;
}


template<Channel_size N>
inline optional<Channel_size>
Channel_alternative::select(Channel_operation (&ops)[N], Goroutine::Handle g)
{
    return select(begin(ops), end(ops), g);
}


inline optional<Channel_size>
Channel_alternative::select(Channel_operation* first, Channel_operation* last, Goroutine::Handle g)
{
    using std::make_shared;

    if (!(pimpl && pimpl.unique()))
        pimpl = make_shared<Impl>();

    return write().select(pimpl, first, last, g);
}


inline Channel_size
Channel_alternative::selected() const
{
    return read().selected();
}


template<Channel_size N>
inline optional<Channel_size>
Channel_alternative::try_select(Channel_operation (&ops)[N])
{
    write().try_select(begin(ops), end(ops));
}


inline Channel_alternative::Impl&
Channel_alternative::write()
{
    using std::make_shared;

    if (!(pimpl && pimpl.unique()))
        pimpl = make_shared<Impl>();

    return *pimpl;
}


/*
    Channel Alternative Implementation
*/
inline
Channel_alternative::Impl::Impl()
    : waiting{nullptr}
{
}


inline Channel_size
Channel_alternative::Impl::selected() const
{
    return *chosen;
}


}   // Implementation Details


/*
    Channel Select
*/
template<Channel_size N>
inline Channel_select::Awaitable
Channel_select::operator()(Channel_operation (&ops)[N])
{
    return Awaitable(&alt, begin(ops), end(ops));
}


/*
    Channel Select Awaitable
*/
inline
Channel_select::Awaitable::Awaitable(Detail::Channel_alternative* ap, Channel_operation* first, Channel_operation* last)
    : altp(ap)
    , firstp(first)
    , lastp(last)
{
}


inline bool
Channel_select::Awaitable::await_ready()
{
    return false;
}


inline Channel_size
Channel_select::Awaitable::await_resume()
{
    return altp->selected();
}


inline bool
Channel_select::Awaitable::await_suspend(Goroutine::Handle g)
{
    return !altp->select(firstp, lastp, g);
}


/*
    Non-Blocking Channel Select
*/
template<Channel_size N>
inline optional<Channel_size>
Channel_try_select::operator()(Channel_operation (&ops)[N])
{
    return alt.try_select(ops);
}


/*
    Channel
*/
template<class T>
Channel<T>
make_channel(Channel_size n)
{
    using std::make_shared;

    return make_shared<Channel<T>::Impl>(n);
}


/*
    Goroutine Launcher
*/
template<class GoFun, class... Args>
inline void
go(GoFun&& fun, Args&&... args)
{
    using std::forward;
    using std::move;

    Goroutine g = fun(forward<Args>(args)...);
    scheduler.submit(move(g));
}


}  // Concurrency
}  // Isptech

