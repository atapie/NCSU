/******************************************************************************
* Single Author info:
* 	tthai 		Thanh Lam 	Thai
*
* Group info:
*	tthai 		Thanh Lam 	Thai
* 	bradhak 	Balaji 		Radhakrishnan
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

// Run configurations
#define BLOCKING 1
#define SINGLE_CALL_REDUCTION 1

/* The number of grid points */
#define   NGRID           100
/* first grid point */
#define   XI              1.0
/* last grid point */
#define   XF              100.0

/* floating point precision type definitions */
typedef   double   FP_PREC;

/* function declarations */
FP_PREC     fn(FP_PREC);
FP_PREC     dfn(FP_PREC);
FP_PREC     ifn(FP_PREC, FP_PREC);
void        print_function_data(int, FP_PREC, FP_PREC*);
void        print_error_data(int np, FP_PREC, FP_PREC, FP_PREC, FP_PREC, FP_PREC*);
int         main(int, char**);

int main (int argc, char *argv[])
{
  int procid, num_procs;
  MPI_Status status;
  // derivative_time, integral_time, err_time is the local sum of runtime for each computation
  // tick is used to mark time
  double derivative_time = 0, integral_time = 0, err_time = 0, tick;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &procid);
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

  // Calculate grid-points per process
  if(NGRID % num_procs > 0)
  {
	if(procid == 0) printf("NGRID should be divisible by the number of processes!");
	MPI_Finalize();
	return 1;
  }
  int points_per_node = NGRID / num_procs;

  //loop index
  int i;

  //domain array and step size
  FP_PREC xc[points_per_node], dx;

  //function array and derivative
  //the size will be dependent on the
  //number of processors used
  //to the program
  FP_PREC yc[points_per_node], dyc[points_per_node];
  
  //integration values
  FP_PREC local_intg, intg;

  //error analysis array
  FP_PREC derr[points_per_node];

  //error analysis values
  FP_PREC dlocal_sum_err, davg_err, dlocal_std_dev, dstd_dev, intg_err;

  //calculate dx
  dx = (FP_PREC)(XF - XI)/(FP_PREC)(NGRID - 1);

  // get start X for each process (my_XI)
  int bins_before_me = procid * points_per_node;
  FP_PREC my_XI = XI + bins_before_me * dx;

  //construct grid
  for (i = 0; i < points_per_node; ++i)
  {
    xc[i] = my_XI + i * dx;
  }

  //define the function
  for(i = 0; i < points_per_node; ++i)
  {
    yc[i] = fn(xc[i]);
  }

  //define holders for left and right bound value
  FP_PREC left_bound_yc, right_bound_yc;
  if(procid == 0) left_bound_yc = fn(XI-dx);
  if(procid == num_procs - 1) right_bound_yc = fn(XF+dx);

  tick = MPI_Wtime();
#if BLOCKING
  if(procid == 0) printf("Using blocking message! \n");
  //Step 1: even nodes send to the right then receive back
  //Step 2: even nodes receive from the left then send back
  if(procid % 2 == 0)
  {
    if(procid < num_procs - 1)
    {
	MPI_Send(&yc[points_per_node-1], 1, MPI_DOUBLE, procid+1, 0, MPI_COMM_WORLD);
	MPI_Recv(&right_bound_yc, 1, MPI_DOUBLE, procid+1, 0, MPI_COMM_WORLD, &status);
    }
    if(procid > 0)
    {
	MPI_Recv(&left_bound_yc, 1, MPI_DOUBLE, procid-1, 0, MPI_COMM_WORLD, &status);
	MPI_Send(&yc[0], 1, MPI_DOUBLE, procid-1, 0, MPI_COMM_WORLD);
    }
  } else
  {
    MPI_Recv(&left_bound_yc, 1, MPI_DOUBLE, procid-1, 0, MPI_COMM_WORLD, &status);
    MPI_Send(&yc[0], 1, MPI_DOUBLE, procid-1, 0, MPI_COMM_WORLD);
    if(procid < num_procs - 1)
    {
    	MPI_Send(&yc[points_per_node-1], 1, MPI_DOUBLE, procid+1, 0, MPI_COMM_WORLD);
    	MPI_Recv(&right_bound_yc, 1, MPI_DOUBLE, procid+1, 0, MPI_COMM_WORLD, &status);
    }
  }
#else
  if(procid == 0) printf("Using non-blocking message! \n");
  MPI_Request request[4];
  int current_request = 0;
  if(procid < num_procs - 1)
  { // receive right bound yc
      MPI_Irecv(&right_bound_yc, 1, MPI_DOUBLE, procid+1, 0, MPI_COMM_WORLD, &request[current_request]);
      ++current_request;
  }
  if(procid > 0)
  { // receive left bound yc
      MPI_Irecv(&left_bound_yc, 1, MPI_DOUBLE, procid-1, 0, MPI_COMM_WORLD, &request[current_request]);
      ++current_request;
  }
  if(procid < num_procs - 1)
  { // send right bound yc to right node
      MPI_Isend(&yc[points_per_node-1], 1, MPI_DOUBLE, procid+1, 0, MPI_COMM_WORLD, &request[current_request]);
      ++current_request;
  }
  if(procid > 0)
  { // send left bound yc to left node
      MPI_Isend(&yc[0], 1, MPI_DOUBLE, procid-1, 0, MPI_COMM_WORLD, &request[current_request]);
      ++current_request;
  }
#endif
  derivative_time += MPI_Wtime() - tick;
  integral_time += MPI_Wtime() - tick;

  // Overlap computation and communication BEGIN
  //compute the derivative using first-order finite differencing
  tick = MPI_Wtime();
  for (i = 1; i < points_per_node-1; ++i)
  {
    dyc[i] = (yc[i + 1] - yc[i - 1])/(2.0 * dx);
  }
  derivative_time += MPI_Wtime() - tick;

  //compute the integral using Trapazoidal rule
  tick = MPI_Wtime();
  local_intg = 0.0;
  for (i = 0; i < points_per_node-1; ++i)
  {
    local_intg += 0.5 * (yc[i] + yc[i + 1]) * dx;
  }
  integral_time += MPI_Wtime() - tick;
  // Overlap computation and communication END

  // WAIT for non-blocking message complete before continue
#if !BLOCKING
  tick = MPI_Wtime();
  MPI_Waitall(current_request, request, MPI_STATUSES_IGNORE);
  derivative_time += MPI_Wtime() - tick;
  integral_time += MPI_Wtime() - tick;
#endif

  // compute derivative of boundary points, runtime is not counted because it's quite small
  dyc[0] = (yc[1] - left_bound_yc)/(2.0 * dx);
  dyc[points_per_node-1] = (right_bound_yc - yc[points_per_node-2])/(2.0 * dx);

  // compute integral at right boundary point, runtime is not counted because it's quite small
  if(procid < num_procs-1) local_intg += 0.5 * (yc[points_per_node-1] + right_bound_yc) * dx;

  tick = MPI_Wtime();
  //compute the error, average error of the derivatives
  for(i = 0; i < points_per_node; ++i)
  {
    if(dfn(xc[i]) == 0)
    {
      printf("WARNING: derivative at point %d on process %d is zero.\n", i, procid);
      derr[i] = 0;
    }
    else derr[i] = fabs((dyc[i] - dfn(xc[i]))/dfn(xc[i]));
  }

  //find the local average error
  dlocal_sum_err = 0.0;
  for(i = 0; i < points_per_node; ++i)
  {
    dlocal_sum_err += derr[i];
  }

  //calculate and output errors
#if SINGLE_CALL_REDUCTION
  if(procid == 0) printf("Using single call reduction! \n");
  //all nodes collect sum err and convert it to the mean value
  MPI_Allreduce(&dlocal_sum_err, &davg_err, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  davg_err /= (FP_PREC)NGRID; // each process calculates global average
#else
  if(procid == 0) printf("Using manual call reduction! \n");
  //all nodes collect sum err and convert it to the mean value
  if(procid != 0) MPI_Send(&dlocal_sum_err, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
  else if(procid == 0)
  {
    davg_err = dlocal_sum_err;
    for(i = 1; i < num_procs; ++i)
    {
      MPI_Recv(&dlocal_sum_err, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
      davg_err += dlocal_sum_err;
    }
    davg_err /= (FP_PREC)NGRID;
  }
  MPI_Bcast(&davg_err, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

  //now all nodes have davg_err, find sum squared differences of local derr
  dlocal_std_dev = 0.0;
  for(i = 0; i < points_per_node; ++i)
  {
    dlocal_std_dev += pow(derr[i] - davg_err, 2);
  }
  err_time += MPI_Wtime() - tick;

#if SINGLE_CALL_REDUCTION
  //reduce local integral & local (sum squared differences of derr) to root
  tick = MPI_Wtime();
  MPI_Reduce(&dlocal_std_dev, &dstd_dev, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  err_time += MPI_Wtime() - tick;
  tick = MPI_Wtime();
  MPI_Reduce(&local_intg, &intg, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  integral_time += MPI_Wtime() - tick;
#else
  //reduce local integral & local (sum squared differences of derr) to root
  if(procid != 0)
  {
    tick = MPI_Wtime();
    MPI_Send(&dlocal_std_dev, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    err_time += MPI_Wtime() - tick;
    tick = MPI_Wtime();
    MPI_Send(&local_intg, 1, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
    integral_time += MPI_Wtime() - tick;
  } else if(procid == 0)
  {
    dstd_dev = dlocal_std_dev;
    intg = local_intg;
    tick = MPI_Wtime();
    for(i = 1; i < num_procs; ++i)
    {
      MPI_Recv(&dlocal_std_dev, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
      dstd_dev += dlocal_std_dev;
    }
    err_time += MPI_Wtime() - tick;
    tick = MPI_Wtime();
    for(i = 1; i < num_procs; ++i)
    {
      MPI_Recv(&local_intg, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
      intg+= local_intg;
    }
    integral_time += MPI_Wtime() - tick;
  }
#endif

  // print out the max runtime for each calculation
  double max_derivative_time, max_integral_time, max_err_time;
  MPI_Reduce(&derivative_time, &max_derivative_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
  MPI_Reduce(&integral_time, &max_integral_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
  MPI_Reduce(&err_time, &max_err_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
  if(procid == 0)
  {
    printf("Max runtime to calculate derivatives is %e\n", max_derivative_time);
    printf("Max runtime to calculate integral is %e\n", max_integral_time);
    printf("Max runtime to calculate derivative errors is %e\n", max_err_time);
  }

  //gather derivative results & errors for output
  //this part shouldn't be included in running time measurements
  FP_PREC *final_dyc = NULL;
  FP_PREC *final_derr = NULL;
  if(procid == 0)
  {
    final_dyc = (FP_PREC*)malloc(NGRID * sizeof(FP_PREC));
    final_derr = (FP_PREC*)malloc(NGRID * sizeof(FP_PREC));
  }
  MPI_Gather(dyc, points_per_node, MPI_DOUBLE, final_dyc, points_per_node, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Gather(derr, points_per_node, MPI_DOUBLE, final_derr, points_per_node, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  //final output at root node (rank 0)
  if(procid == 0)
  {
    dstd_dev = sqrt(dstd_dev/(FP_PREC)NGRID);
    if(ifn(XI, XF) == 0) {
      printf("WARNING: true integral value from XI to XF is equal zero.\n");
      intg_err = 0;
    } else {
      intg_err = fabs((ifn(XI, XF) - intg)/ifn(XI, XF));
    }

    print_function_data(NGRID, dx, final_dyc);
    print_error_data(NGRID, davg_err, dstd_dev, intg_err, dx, final_derr);

    free(final_dyc);
    free(final_derr);
  }

  MPI_Finalize();
  return 0;
}

//prints out the function and its derivative to a file
void print_function_data(int np, FP_PREC dx, FP_PREC *dydx)
{
  int   i;

  FILE *fp = fopen("fn.dat", "w");
  FP_PREC x, y;
  for(i = 0; i < np; ++i)
  {
    x = XI + i * dx;
    y = fn(x);
    fprintf(fp, "%f %f %f\n", x, y, dydx[i]);
  }

  fclose(fp);
}

void print_error_data(int np, FP_PREC avgerr, FP_PREC stdd, FP_PREC ierr, FP_PREC dx, FP_PREC *err)
{
  int   i;
  FILE *fp = fopen("err.dat", "w");

  fprintf(fp, "%e\n%e\n%e\n", avgerr, stdd, ierr);
  FP_PREC x;
  for(i = 0; i < np; ++i)
  {
    x = XI + i * dx;
    fprintf(fp, "%e %e \n", x, err[i]);
  }
  fclose(fp);
}

