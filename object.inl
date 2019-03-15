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
    Object
*/
inline
Object::Object(Object_impl imp, Object_id oid)
    : id{std::move(oid)}
    , impl{std::move(imp)}
{
}


inline
Object::Object(Object&& other)
    : id{std::move(other.id)}
    , impl{std::move(other.impl)}
{
}


inline void
Object::identity(Object_id oid)
{
    id = oid;
}


inline Object_id
Object::identity() const
{
    return id;
}


inline Future<bool>
Object::invoke(Function fun, Const_buffers in, Io_buffer* outp) const
{
    return impl.invoke(id, fun, in, outp);
}


inline Future<bool>
Object::invoke(Function fun, Const_buffers in, Mutable_buffers* outp) const
{
    return impl.invoke(id, fun, in, outp);
}


inline void
Object::implementation(Object_impl imp)
{
    impl = std::move(imp);
}


inline const Object_impl&
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


}   // Orb
}   // Isptech
