#include "header.h"

//---------------------------------------------------------------------
// this function performs the solution of the approximate factorization
// step in the z-direction for all five matrix components
// simultaneously. The Thomas algorithm is employed to solve the
// systems for the z-lines. Boundary conditions are non-periodic
//---------------------------------------------------------------------


#define lhs_(x,y,z,m) lhs_[m + (z) * 5 + (y) * 5 * P_SIZE + (x) * 5 * P_SIZE * P_SIZE]
#define lhsm_(x,y,z,m) lhsm_[m + (z) * 5 + (y) * 5 * P_SIZE + (x) * 5 * P_SIZE * P_SIZE]
#define lhsp_(x,y,z,m) lhsp_[m + (z) * 5 + (y) * 5 * P_SIZE + (x) * 5 * P_SIZE * P_SIZE]
#define rhs(x,y,z,m) rhs[m + (z) * 5 + (y) * 5 * P_SIZE + (x) * 5 * P_SIZE * P_SIZE]
#define rho_i(x,y,z) rho_i[z + (y) * P_SIZE + (x) * P_SIZE * P_SIZE]
#define vs(x,y,z) vs[z + (y) * P_SIZE + (x) * P_SIZE * P_SIZE]
#define speed(x,y,z) speed[z + (y) * P_SIZE + (x) * P_SIZE * P_SIZE]
__global__ void z_solve_kernel_one(double* lhs_, double* lhsp_, double* lhsm_, int nx2, int ny2, int nz2)
{
	int m;

	int i = threadIdx.x + blockIdx.x * blockDim.x + 1;
	int j = threadIdx.y + blockIdx.y * blockDim.y + 1;

	//part 1
	if (j <= ny2 && i <= nx2)
    {        
        for (m = 0; m < 5; m++)
        {
            lhs_(j,i,0,m) = lhs_(j,i,nz2 + 1,m) = 0.0;
            lhsp_(j,i,0,m) = lhsp_(j,i,nz2 + 1,m) = 0.0;
            lhsm_(j,i,0,m) = lhsm_(j,i,nz2 + 1,m) = 0.0;
        }
        lhs_(j,i,0,2) = lhs_(j,i,nz2 + 1,2) = 1.0;
        lhsp_(j,i,0,2) = lhsp_(j,i,nz2 + 1,2) = 1.0;
        lhsm_(j,i,0,2) = lhsm_(j,i,nz2 + 1,2) = 1.0;    
	}
}

void z_solve()
{

    int i, j, k, k1, k2, m;
    double ru1, rhos1, fac1, fac2;

    const int size5 = sizeof(double)*P_SIZE*P_SIZE*P_SIZE*5;
	const int size = sizeof(double)*P_SIZE*P_SIZE*P_SIZE;

	dim3 blocks = dim3(nx2 / 32+1, ny2 / 4+1, nz2);
	dim3 threads = dim3(32, 4, 1);

    dim3 blocks2 = dim3(nx2 / 4+1, ny2);
	dim3 threads2 = dim3(4, 1);

    if (timeron) timer_start(t_zsolve);

    
    cudaDeviceSynchronize();
	z_solve_kernel_one<<<blocks2, threads2>>>((double*)lhs_gpu, (double*)lhsp_gpu, (double*)lhsm_gpu, nx2, ny2, nz2);

    CudaSafeCall(cudaMemcpy(rho_i, gpuRho_i, size, cudaMemcpyDeviceToHost));
	CudaSafeCall(cudaMemcpy(vs, gpuVs, size, cudaMemcpyDeviceToHost));
	CudaSafeCall(cudaMemcpy(speed, gpuSpeed, size, cudaMemcpyDeviceToHost));
	CudaSafeCall(cudaMemcpy(rhs, gpuRhs, size5, cudaMemcpyDeviceToHost));
	CudaSafeCall(cudaMemcpy(lhs_, lhs_gpu, size5, cudaMemcpyDeviceToHost));
	CudaSafeCall(cudaMemcpy(lhsp_, lhsp_gpu, size5, cudaMemcpyDeviceToHost));
	CudaSafeCall(cudaMemcpy(lhsm_, lhsm_gpu, size5, cudaMemcpyDeviceToHost));

    for (j = 1; j <= ny2; j++) 
    {
        for (i = 1; i <= nx2; i++)
        {

            for (k = 1; k <= nz2; k++)
            {
                lhs_[j][i][k][0] = 0.0;

                ru1 = c3c4*rho_i[k - 1][j][i];
                rhos1 = max(max(dz4 + con43*ru1, dz5 + c1c5*ru1), max(dzmax + ru1, dz1));
                lhs_[j][i][k][1] = -dttz2 * ws[k - 1][j][i] - dttz1 * rhos1;

                ru1 = c3c4*rho_i[k][j][i];
                rhos1 = max(max(dz4 + con43*ru1, dz5 + c1c5*ru1), max(dzmax + ru1, dz1));
                lhs_[j][i][k][2] = 1.0 + c2dttz1 * rhos1;

                ru1 = c3c4*rho_i[k + 1][j][i];
                rhos1 = max(max(dz4 + con43*ru1, dz5 + c1c5*ru1), max(dzmax + ru1, dz1));
                lhs_[j][i][k][3] = dttz2 * ws[k + 1][j][i] - dttz1 * rhos1;
                lhs_[j][i][k][4] = 0.0;

                if (k == 1)
                {
                    lhs_[j][i][k][2] = lhs_[j][i][k][2] + comz5;
                    lhs_[j][i][k][3] = lhs_[j][i][k][3] - comz4;
                    lhs_[j][i][k][4] = lhs_[j][i][k][4] + comz1;
                }
                else if (k == 2)
                {
                    lhs_[j][i][k][1] = lhs_[j][i][k][1] - comz4;
                    lhs_[j][i][k][2] = lhs_[j][i][k][2] + comz6;
                    lhs_[j][i][k][3] = lhs_[j][i][k][3] - comz4;
                    lhs_[j][i][k][4] = lhs_[j][i][k][4] + comz1;
                }
                else if (k == nz2 - 1)
                {
                    lhs_[j][i][k][0] = lhs_[j][i][k][0] + comz1;
                    lhs_[j][i][k][1] = lhs_[j][i][k][1] - comz4;
                    lhs_[j][i][k][2] = lhs_[j][i][k][2] + comz6;
                    lhs_[j][i][k][3] = lhs_[j][i][k][3] - comz4;
                }
                else if (k == nz2)
                {
                    lhs_[j][i][k][0] = lhs_[j][i][k][0] + comz1;
                    lhs_[j][i][k][1] = lhs_[j][i][k][1] - comz4;
                    lhs_[j][i][k][2] = lhs_[j][i][k][2] + comz5;
                }
                else
                {
                    lhs_[j][i][k][0] = lhs_[j][i][k][0] + comz1;
                    lhs_[j][i][k][1] = lhs_[j][i][k][1] - comz4;
                    lhs_[j][i][k][2] = lhs_[j][i][k][2] + comz6;
                    lhs_[j][i][k][3] = lhs_[j][i][k][3] - comz4;
                    lhs_[j][i][k][4] = lhs_[j][i][k][4] + comz1;
                }

                lhsp_[j][i][k][0] = lhs_[j][i][k][0];
                lhsp_[j][i][k][1] = lhs_[j][i][k][1] - dttz2 * speed[k - 1][j][i];
                lhsp_[j][i][k][2] = lhs_[j][i][k][2];
                lhsp_[j][i][k][3] = lhs_[j][i][k][3] + dttz2 * speed[k + 1][j][i];
                lhsp_[j][i][k][4] = lhs_[j][i][k][4];
                lhsm_[j][i][k][0] = lhs_[j][i][k][0];
                lhsm_[j][i][k][1] = lhs_[j][i][k][1] + dttz2 * speed[k - 1][j][i];
                lhsm_[j][i][k][2] = lhs_[j][i][k][2];
                lhsm_[j][i][k][3] = lhs_[j][i][k][3] - dttz2 * speed[k + 1][j][i];
                lhsm_[j][i][k][4] = lhs_[j][i][k][4];
            }

            for (k = 1; k <= nz2; k++)
            {
                k1 = k;
                k2 = k + 1;

                fac1 = 1.0 / lhs_[j][i][k - 1][2];
                lhs_[j][i][k - 1][3] = fac1 * lhs_[j][i][k - 1][3];
                lhs_[j][i][k - 1][4] = fac1 * lhs_[j][i][k - 1][4];
                for (m = 0; m < 3; m++)
                    rhs[k - 1][j][i][m] = fac1 * rhs[k - 1][j][i][m];

                lhs_[j][i][k1][2] = lhs_[j][i][k1][2] - lhs_[j][i][k1][1] * lhs_[j][i][k - 1][3];
                lhs_[j][i][k1][3] = lhs_[j][i][k1][3] - lhs_[j][i][k1][1] * lhs_[j][i][k - 1][4];
                for (m = 0; m < 3; m++)
                    rhs[k1][j][i][m] = rhs[k1][j][i][m] - lhs_[j][i][k1][1] * rhs[k - 1][j][i][m];

                lhs_[j][i][k2][1] = lhs_[j][i][k2][1] - lhs_[j][i][k2][0] * lhs_[j][i][k - 1][3];
                lhs_[j][i][k2][2] = lhs_[j][i][k2][2] - lhs_[j][i][k2][0] * lhs_[j][i][k - 1][4];
                for (m = 0; m < 3; m++)
                    rhs[k2][j][i][m] = rhs[k2][j][i][m] - lhs_[j][i][k2][0] * rhs[k - 1][j][i][m];

                if (k == nz2)
                {
                    fac1 = 1.0 / lhs_[j][i][k1][2];
                    lhs_[j][i][k1][3] = fac1 * lhs_[j][i][k1][3];
                    lhs_[j][i][k1][4] = fac1 * lhs_[j][i][k1][4];
                    for (m = 0; m < 3; m++)
                        rhs[k1][j][i][m] = fac1 * rhs[k1][j][i][m];

                    lhs_[j][i][k2][2] = lhs_[j][i][k2][2] - lhs_[j][i][k2][1] * lhs_[j][i][k1][3];
                    lhs_[j][i][k2][3] = lhs_[j][i][k2][3] - lhs_[j][i][k2][1] * lhs_[j][i][k1][4];
                    for (m = 0; m < 3; m++)
                        rhs[k2][j][i][m] = rhs[k2][j][i][m] - lhs_[j][i][k2][1] * rhs[k1][j][i][m];

                    fac2 = 1.0 / lhs_[j][i][k2][2];
                    for (m = 0; m < 3; m++)
                        rhs[k2][j][i][m] = fac2 * rhs[k2][j][i][m];
                }

                m = 3;
                fac1 = 1.0 / lhsp_[j][i][k - 1][2];
                lhsp_[j][i][k - 1][3] = fac1 * lhsp_[j][i][k - 1][3];
                lhsp_[j][i][k - 1][4] = fac1 * lhsp_[j][i][k - 1][4];
                rhs[k - 1][j][i][m] = fac1 * rhs[k - 1][j][i][m];

                lhsp_[j][i][k1][2] = lhsp_[j][i][k1][2] - lhsp_[j][i][k1][1] * lhsp_[j][i][k - 1][3];
                lhsp_[j][i][k1][3] = lhsp_[j][i][k1][3] - lhsp_[j][i][k1][1] * lhsp_[j][i][k - 1][4];
                rhs[k1][j][i][m] = rhs[k1][j][i][m] - lhsp_[j][i][k1][1] * rhs[k - 1][j][i][m];

                lhsp_[j][i][k2][1] = lhsp_[j][i][k2][1] - lhsp_[j][i][k2][0] * lhsp_[j][i][k - 1][3];
                lhsp_[j][i][k2][2] = lhsp_[j][i][k2][2] - lhsp_[j][i][k2][0] * lhsp_[j][i][k - 1][4];
                rhs[k2][j][i][m] = rhs[k2][j][i][m] - lhsp_[j][i][k2][0] * rhs[k - 1][j][i][m];

                m = 4;
                fac1 = 1.0 / lhsm_[j][i][k - 1][2];
                lhsm_[j][i][k - 1][3] = fac1 * lhsm_[j][i][k - 1][3];
                lhsm_[j][i][k - 1][4] = fac1 * lhsm_[j][i][k - 1][4];
                rhs[k - 1][j][i][m] = fac1 * rhs[k - 1][j][i][m];

                lhsm_[j][i][k1][2] = lhsm_[j][i][k1][2] - lhsm_[j][i][k1][1] * lhsm_[j][i][k - 1][3];
                lhsm_[j][i][k1][3] = lhsm_[j][i][k1][3] - lhsm_[j][i][k1][1] * lhsm_[j][i][k - 1][4];
                rhs[k1][j][i][m] = rhs[k1][j][i][m] - lhsm_[j][i][k1][1] * rhs[k - 1][j][i][m];

                lhsm_[j][i][k2][1] = lhsm_[j][i][k2][1] - lhsm_[j][i][k2][0] * lhsm_[j][i][k - 1][3];
                lhsm_[j][i][k2][2] = lhsm_[j][i][k2][2] - lhsm_[j][i][k2][0] * lhsm_[j][i][k - 1][4];
                rhs[k2][j][i][m] = rhs[k2][j][i][m] - lhsm_[j][i][k2][0] * rhs[k - 1][j][i][m];

                if (k == nz2)
                {
                    m = 3;
                    fac1 = 1.0 / lhsp_[j][i][k1][2];
                    lhsp_[j][i][k1][3] = fac1 * lhsp_[j][i][k1][3];
                    lhsp_[j][i][k1][4] = fac1 * lhsp_[j][i][k1][4];
                    rhs[k1][j][i][m] = fac1 * rhs[k1][j][i][m];

                    lhsp_[j][i][k2][2] = lhsp_[j][i][k2][2] - lhsp_[j][i][k2][1] * lhsp_[j][i][k1][3];
                    lhsp_[j][i][k2][3] = lhsp_[j][i][k2][3] - lhsp_[j][i][k2][1] * lhsp_[j][i][k1][4];
                    rhs[k2][j][i][m] = rhs[k2][j][i][m] - lhsp_[j][i][k2][1] * rhs[k1][j][i][m];

                    m = 4;
                    fac1 = 1.0 / lhsm_[j][i][k1][2];
                    lhsm_[j][i][k1][3] = fac1 * lhsm_[j][i][k1][3];
                    lhsm_[j][i][k1][4] = fac1 * lhsm_[j][i][k1][4];
                    rhs[k1][j][i][m] = fac1 * rhs[k1][j][i][m];

                    lhsm_[j][i][k2][2] = lhsm_[j][i][k2][2] - lhsm_[j][i][k2][1] * lhsm_[j][i][k1][3];
                    lhsm_[j][i][k2][3] = lhsm_[j][i][k2][3] - lhsm_[j][i][k2][1] * lhsm_[j][i][k1][4];
                    rhs[k2][j][i][m] = rhs[k2][j][i][m] - lhsm_[j][i][k2][1] * rhs[k1][j][i][m];

                    rhs[k2][j][i][3] = rhs[k2][j][i][3] / lhsp_[j][i][k2][2];
                    rhs[k2][j][i][4] = rhs[k2][j][i][4] / lhsm_[j][i][k2][2];

                    for (m = 0; m < 3; m++)
                        rhs[k1][j][i][m] = rhs[k1][j][i][m] - lhs_[j][i][k1][3] * rhs[k2][j][i][m];

                    rhs[k1][j][i][3] = rhs[k1][j][i][3] - lhsp_[j][i][k1][3] * rhs[k2][j][i][3];
                    rhs[k1][j][i][4] = rhs[k1][j][i][4] - lhsm_[j][i][k1][3] * rhs[k2][j][i][4];
                }
            }
        }
    }

	for (j = 1; j <= ny2; j++) 
    {
        for (i = 1; i <= nx2; i++)
        {
			for (k = nz2; k >= 1; k--)
            {
                k1 = k;
                k2 = k + 1;

                for (m = 0; m < 3; m++)
                    rhs[k - 1][j][i][m] = rhs[k - 1][j][i][m] - lhs_[j][i][k - 1][3] * rhs[k1][j][i][m] - lhs_[j][i][k - 1][4] * rhs[k2][j][i][m];

                rhs[k - 1][j][i][3] = rhs[k - 1][j][i][3] - lhsp_[j][i][k - 1][3] * rhs[k1][j][i][3] - lhsp_[j][i][k - 1][4] * rhs[k2][j][i][3];
                rhs[k - 1][j][i][4] = rhs[k - 1][j][i][4] - lhsm_[j][i][k - 1][3] * rhs[k1][j][i][4] - lhsm_[j][i][k - 1][4] * rhs[k2][j][i][4];
            }
        }
    }

    //---------------------------------------------------------------------
    // block-diagonal matrix-vector multiplication                       
    //---------------------------------------------------------------------
    double t1, t2, t3, ac, xvel, yvel, zvel;
    double btuz, ac2u, uzik1, r1, r2, r3, r4, r5;
    if (timeron) timer_start(t_tzetar);

    for (k = 1; k <= nz2; k++)
    {
        for (j = 1; j <= ny2; j++)
        {
            for (i = 1; i <= nx2; i++)
            {
                xvel = us[k][j][i];
                yvel = vs[k][j][i];
                zvel = ws[k][j][i];
                ac = speed[k][j][i];

                ac2u = ac*ac;

                r1 = rhs[k][j][i][0];
                r2 = rhs[k][j][i][1];
                r3 = rhs[k][j][i][2];
                r4 = rhs[k][j][i][3];
                r5 = rhs[k][j][i][4];

                uzik1 = u[k][j][i][0];
                btuz = bt * uzik1;

                t1 = btuz / ac * (r4 + r5);
                t2 = r3 + t1;
                t3 = btuz * (r4 - r5);

                rhs[k][j][i][0] = t2;
                rhs[k][j][i][1] = -uzik1*r2 + xvel*t2;
                rhs[k][j][i][2] = uzik1*r1 + yvel*t2;
                rhs[k][j][i][3] = zvel*t2 + t3;
                rhs[k][j][i][4] = uzik1*(-xvel*r2 + yvel*r1) + qs[k][j][i] * t2 + c2iv*ac2u*t1 + zvel*t3;
            }
        }
    }
    if (timeron) timer_stop(t_tzetar);
    if (timeron) timer_stop(t_zsolve);
}
