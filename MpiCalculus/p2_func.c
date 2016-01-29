/******************************************************************************
* Single Author info:
* 	tthai 		Thanh Lam 	Thai
*
* Group info:
*	tthai 		Thanh Lam 	Thai
* 	bradhak 	Balaji 		Radhakrishnan
******************************************************************************/
#include <math.h>

/* floating point precision type definitions */
typedef   double   FP_PREC;

//returns the function y(x) = fn
FP_PREC fn(FP_PREC x)
{
  return sqrt(x);
  //return sin(x);
//  return x;
}

//returns the derivative d(fn)/dx = dy/dx
FP_PREC dfn(FP_PREC x)
{
  return 0.5*(1.0/sqrt(x));
  //return cos(x);
//  return 1;
}

//returns the integral from a to b of y(x) = fn
FP_PREC ifn(FP_PREC a, FP_PREC b)
{
  return (2./3.) * (pow(sqrt(b), 3) - pow(sqrt(a),3));
  //return cos(a) - cos(b);
//  return 0.5 * (b*b - a*a);
}

