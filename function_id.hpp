//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/function_id.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/function_id.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_FUNCTION_ID_HPP
#define ISPTECH_ORB_FUNCTION_ID_HPP

#include "boost/operators.hpp"


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Names/Types
*/
using Function_name = int;


/*
    Function Type
*/
enum class Function_type : int {
    idempotent,
    non_idempotent
};


/*
    Function ID
*/
class Function_id : boost::totally_ordered<Function_id> {
public:
    // Construct
    constexpr Function_id();
    constexpr Function_id(Function_name, Function_type);

    // Modifiers
    constexpr void name(Function_name);
    constexpr void type(Function_type);

    // Observers
    constexpr Function_name name() const;
    constexpr Function_type type() const;

    // Comparisons
    friend constexpr bool operator==(Function_id, Function_id);
    friend constexpr bool operator< (Function_id, Function_id);

private:
    // Data
    Function_name which;
    Function_type kind;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_FUNCTION_ID_HPP

//  $CUSTOM_FOOTER$
