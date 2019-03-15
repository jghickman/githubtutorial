//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/object_impl.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/object_impl.inl,v $
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
    Object Implementation
*/
template<class T>
inline
Object_impl::Object_impl(Interface_ptr p)
    : ifacep{std::move(p)}
{
}


template<class T>
inline Future<bool>
Object_impl::invoke(Object_id obj, Function fun, Const_buffers in, Io_buffer* outp)
{
    return ifacep->invoke(obj, fun, in, outp);
}


template<class T>
inline Future<bool>
Object_impl::invoke(Object_id obj, Function fun, Const_buffers in, Mutable_buffers* outp)
{
    return ifacep->invoke(obj, fun, in, outp);
}


inline Object_impl&
Object_impl::operator=(Object_impl other)
{
    swap(*this, other);
    return *this;
}


inline bool
operator==(const Object_impl& x, const Object_impl& y)
{
    return x.ifacep == y.ifacep;
}


inline bool
operator< (const Object_impl& x, const Object_impl& y)
{
    return x.ifacep < y.ifacep;
}


inline void
swap(Object_impl& x, Object_impl& y)
{
    using std::swap;

    swap(x.ifacep, y.ifacep);
}


}   // Orb
}   // Isptech
