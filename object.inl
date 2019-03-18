//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/object.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/object.inl,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Object Dispatcher
*/
template<class T>
inline
Object_dispatcher::Object_dispatcher(Interface_ptr p)
    : ifacep{std::move(p)}
{
}


inline
Object_dispatcher::Object_dispatcher(Object_dispatcher&& other)
    : ifacep{std::move(other.ifacep)}
{
}


template<class T>
inline Future<bool>
Object_dispatcher::invoke(Object_id obj, Function_id fun, Io_buffer* iop, const Object_map& map)
{
    return ifacep->invoke(obj, fun, iop, map);
}


inline Object_dispatcher&
Object_dispatcher::operator=(Object_dispatcher other)
{
    swap(*this, other);
    return *this;
}


inline
Object_dispatcher::operator bool() const
{
    return ifacep ? true : false;
}


inline bool
operator==(const Object_dispatcher& x, const Object_dispatcher& y)
{
    return x.ifacep == y.ifacep;
}


inline bool
operator< (const Object_dispatcher& x, const Object_dispatcher& y)
{
    return x.ifacep < y.ifacep;
}


inline void
swap(Object_dispatcher& x, Object_dispatcher& y)
{
    using std::swap;

    swap(x.ifacep, y.ifacep);
}


/*
    Object
*/
inline
Object::Object(Object_dispatcher disp, Object_id id)
    : impl{std::move(imp)}
    , obj{std::move(id)}
{
}


inline
Object::Object(Object&& other)
    : impl{std::move(other.impl)}
    , obj{std::move(other.obj)}
{
}


inline void
Object::dispatcher(Object_dispatcher disp)
{
    impl = std::move(disp);
}


inline const Object_dispatcher&
Object::dispatcher() const
{
    return impl;
}


inline void
Object::identity(Object_id id)
{
    obj = id;
}


inline Object_id
Object::identity() const
{
    return id;
}


inline Future<bool>
Object::invoke(Function fun, Io_buffer* iop, const Object_map& objs) const
{
    return impl.invoke(obj, fun, iop, objects);
}


inline void
Object::implementation(Object_dispatcher imp)
{
    impl = std::move(imp);
}


inline const Object_dispatcher&
Object::implementation() const
{
    return impl;
}


inline Object&
Object::operator=(Object other)
{
    swap(*this, other);
    return *this;
}


inline bool
operator==(const Object& x, const Object& y)
{
    if (x.id != y.id) return false;
    if (x.impl != y.impl) return false;
    return true;
}


inline bool
operator< (const Object& x, const Object& y)
{
    if (x.id < y.id) return true;
    if (y.id < x.id) return false;
    if (x.impl < y.impl) return true;
    if (y.impl < x.impl) return false;
    return true;
}


inline void
swap(Object& x, Object& y)
{
    using std::swap;

    swap(x.id, y.id);
    swap(x.impl, y.impl);
}


/*
    Object Map
*/
inline
Object_map::Object_map(Interface_ptr p)
    : ifacep{std::move(p)}
{
}


inline Object
Object_map::find(Object_id id) const
{ 
    return ifacep->find(id);
}


inline void
Object_map::insert(Object obj)
{
    ifacep->insert(std::move(obj));
}


inline
Object_map::operator bool() const
{
    return ifacep ? true : false;
}


inline Object
Object_map::remove(Object_id id)
{
    return ifacep->remove(id);
}


inline bool
operator==(const Object_map& x, const Object_map& y)
{
    return x.ifacep == y.ifacep;
}


inline bool
operator< (const Object_map& x, const Object_map& y)
{
    return x.ifacep < y.ifacep;
}


inline void
swap(Object_map& x, Object_map& y)
{
    swap(x.ifacep, y.ifacep);
}


}   // Orb
}   // Isptech
