//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/endpoint.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/endpoint.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_ENDPOINT_HPP
#define ISPTECH_ORB_ENDPOINT_HPP

#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Endpoint
*/
class Endpoint : boost::totally_ordered<Endpoint> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Endpoint() = default;
    Endpoint(const Endpoint&) = default;
    Endpoint& operator=(const Endpoint&) = default;
    Endpoint(Endpoint&&);
    Endpoint& operator=(Endpoint&&);
    friend void swap(Endpoint&, Endpoint&);

    // Comparisons
    friend bool operator==(const Endpoint&, const Endpoint&);
    friend bool operator< (const Endpoint&, const Endpoint&);

private:
    // Data
    Interface_ptr ifacep;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_ENDPOINT_HPP

//  $CUSTOM_FOOTER$

