//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/identity.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/identity.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_IDENTITY_HPP
#define ISPTECH_ORB_IDENTITY_HPP

#include "boost/operators.hpp"


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Names/Types
*/
using Instance  = long long;
using Type      = long long;


/*
    Constants
*/
const Type      nil_type        = 0;
const Instance  nil_instance    = 0;


/*
    Identity
*/
class Identity : boost::totally_ordered<Identity> {
public:
    // Construct
    constexpr Identity();
    constexpr Identity(Type, Instance);

    // Modifiers
    constexpr void type(Type);
    constexpr void instance(Instance);

    // Observers
    constexpr Type        type() const;
    constexpr Instance    instance() const;

    // Comparisons
    friend constexpr bool operator==(Identity, Identity);
    friend constexpr bool operator< (Identity, Identity);

private:
    // Data
    Type        what;
    Instance    which;
};


/*
    Constants
*/
constexpr Identity nil_identity;


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_IDENTITY_HPP

//  $CUSTOM_FOOTER$
