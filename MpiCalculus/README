### Implement the function analysis code in a parallel scheme:

Given some function, the code will numerically compute derivatives at all grid points, integral of the function over the grid points, and the error and standard deviation of the error of the numerical computation.

### Problem procedure
* Decompose the grid into slices based on processor rank.
* Calculate the derivative of the functions using a finite difference method. To properly calculate the derivative at the domain boundaries, use (a) a blocking point-to-point communication and (b) a non-blocking point-to-point communication.
* Calculate the integral of the function using the trapezoidal rule. Here, we will require again a communication across the boundary, as well as a global reduction sum.
* Calculate the errors in the derivatives and in the integral. Communicate a vector of errors for each grid point to the root node, and have the root node calculate the average error and the standard deviation the derivatives, and the relative error in the calculation of the integral.

The input function must be continuous and smooth - that is, that has an analytic first derivative. Examples include:
* x^2
* sqrt(x)
* sin(2 * x)

Also avoid choosing a function that behaves weird between the user-defined grid values XI and XF. That is, do not do illegal math operations like fn(x) = 1/x on the range with a grid value x = 0.0, or fn(x) = sqrt(x) with a grid value x = -1.0.

The program will output a file err.dat which gives:

    The average error in the derivative calculation;
    The standard deviation of the error in the derivative calculation;
    The relative error of the integral calculation;
    A point-by-point list of the error at each grid point.

Edit the program to include more grid points, different functions, etc.
