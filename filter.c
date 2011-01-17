/* 
FILTER.C 
An ANSI C implementation of MATLAB FILTER.M (built-in)
Written by Chen Yangquan <elecyq@nus.edu.sg>
1998-11-11
*/

#include<stdio.h>
#define ORDER 3
#define NP 1001

/*
void filter(int,float *,float *,int,float *,float *);
*/

filter(int ord, float *a, float *b, int np, float *x, float *y)
{
        int i,j;
	y[0]=b[0]*x[0];
	for (i=1;i<ord+1;i++)
	{
        y[i]=0.0;
        for (j=0;j<i+1;j++)
        	y[i]=y[i]+b[j]*x[i-j];
        for (j=0;j<i;j++)
        	y[i]=y[i]-a[j+1]*y[i-j-1];
	}
/* end of initial part */
for (i=ord+1;i<np+1;i++)
{
	y[i]=0.0;
        for (j=0;j<ord+1;j++)
        y[i]=y[i]+b[j]*x[i-j];
        for (j=0;j<ord;j++)
        y[i]=y[i]-a[j+1]*y[i-j-1];
}
} /* end of filter */







main()
{
 FILE *fp;
 float x[NP],y[NP],a[ORDER+1],b[ORDER+1];
 int i,j;

/* printf("hello world \n"); */

if((fp=fopen("acc1.dat","r"))!=NULL)
{
        for (i=0;i<NP;i++)
        {
         fscanf(fp,"%f",&x[i]);
/*         printf("%f\n",x[i]); */
        }
}
else
{
        printf("\n file not found! \n");
        exit(-1);
}
close(fp);

/*  test coef from
 [b,a]=butter(3,30/500);  in MATLAB
*/
b[0]=0.0007;
b[1]=0.0021;
b[2]=0.0021;
b[3]=0.0007;
a[0]=1.0000;
a[1]=-2.6236;
a[2]=2.3147;
a[3]=-0.6855;

filter(ORDER,a,b,NP,x,y);
/* NOW y=filter(b,a,x);*/

/* reverse the series for FILTFILT */
for (i=0;i<NP;i++)
{ x[i]=y[NP-i-1];}
/* do FILTER again */
filter(ORDER,a,b,NP,x,y);
/* reverse the series back */
for (i=0;i<NP;i++)
{ x[i]=y[NP-i-1];}
for (i=0;i<NP;i++)
{ y[i]=x[i];}
/* NOW y=filtfilt(b,a,x); boundary handling not included*/

if((fp=fopen("acc10.dat","w+"))!=NULL)
{
        for (i=0;i<NP;i++)
        {
         fprintf(fp,"%f\n",y[i]);
        }
}
else
{
        printf("\n file cannot be created! \n");
        exit(-1);
}
close(fp);
}  
/* end of filter.c */


