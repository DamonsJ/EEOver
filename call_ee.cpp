#include <stdio.h>

#include<time.h>
#include <sys/time.h>
#include <iostream>


#include <deque>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/foreach.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include "program_constants.h"


typedef boost::geometry::model::d2::point_xy<double,        
                                             boost::geometry::cs::cartesian> point_2d;
typedef boost::geometry::model::polygon<point_2d> polygon_2d;
using namespace std;
using namespace boost::geometry;


#define MIN(a,b) ((a<b)?a:b)
#define MAX(a,b) ((a<b)?b:a)

const int n = 20;

double ellipse_ellipse_overlap (double PHI_1, double A1, double B1, 
                                double H1, double K1, double PHI_2, 
                                double A2, double B2, double H2, double K2, 
                                double X[4], double Y[4], int * nroots,
                                int *rtnCode);
                             

//putting current date in a filename
//http://stackoverflow.com/questions/5138913/c-putting-current-date-in-a-filename
void  setFileName(char * name)
{  
    time_t now;
    struct tm *today;  
    char date[9];
    //get current date  
    time(&now);  
    today = localtime(&now);
    //print it in DD_MM_YY_H_M_S format.
    strftime(date, 25, "%d_%m_%Y_%H_%M_%S", today);
    strcat(name, "_");
    strcat(name, date);
    strcat(name, ".txt");
}

//convert ellipse to polygon
int ellipse2poly(float PHI_1, float A1, float B1, float H1, float K1, polygon_2d * po)
{
    //using namespace boost::geometry;
    polygon_2d  poly;
    
    float xc = H1;
    float yc = K1;
    float a = A1;
    float b = B1;
    float w = PHI_1;
    //	----
    if(!n)
    {
        std::cout << "error ellipse(): nshould be >0\n" <<std::endl;
        return 0;
    }
    float t = 0;
    int i = 0;
    double coor[n*2+1][2];
  
    double x, y;
    float step = pi/n;
    float sinphi = sin(w);
    float cosphi = cos(w);
    //std::cout << "step: " << step << "  sin=  " << sinphi << "  cos= " << cosphi << std::endl;
    for(i=0; i<2*n+1; i++)
    {   
        x = xc + a*cos(t)*cosphi - b*sin(t)*sinphi;
        y = yc + a*cos(t)*sinphi + b*sin(t)*cosphi;
        coor[i][0] = x;
        coor[i][1] = y;
        t += step;
    }
    assign_points(poly, coor);
    correct(poly);
    *po = poly;
    return 1;
}

// overlaping area of two polygons
float getOverlapingAreaPoly(polygon_2d poly, polygon_2d poly2)
{
    float overAreaPoly = 0.0;    
    std::deque<polygon_2d> output;
    boost::geometry::intersection(poly, poly2, output);

    //int i = 0;

    BOOST_FOREACH(polygon_2d const& p, output)
    {
        overAreaPoly = boost::geometry::area(p);
        //std::cout << i++ << ": " << std::endl;
    }
    return overAreaPoly;
}

// real timestamping
double get_time(void)
{
    struct timeval stime;
    gettimeofday (&stime, (struct timezone*)0);
    return (stime.tv_sec+((double)stime.tv_usec)/1000000);
}

int main (int argc, char ** argv) 
{
    double A1, B1, H1, K1, PHI_1;
    double A2, B2, H2, K2, PHI_2;
    double area;
    int rtn;
    if (argc < 2)
    {
        printf("Usage: %s inputfile\n exit...\n", argv[0]);
        exit(-1);
    }
    printf ("Calling ellipse_ellipse_overlap.c\n\n");

    int MAXITER = 1, i; //MAXITER: to compare the run-time of poly- and analytical solution.
    double t_se, t_fe;
    double t_sp, t_fp;

    polygon_2d poly, poly2;
    float areaPoly;
    float eps = 0.0001, err;
    float time_e = 0;
    float time_p = 0;
    //float Re, Rp, Oe, Op;	

    char inputFile[100] = "";
    strcpy(inputFile,argv[1]);//"testcases.txt"; 
    char resultFile[100] = "results";
    
    setFileName(resultFile);
    printf("resultFile = <%s>\n", resultFile);

    char rootsFile[100] = "roots";
    setFileName(rootsFile);
    printf("rootsFile = <%s>\n", rootsFile);

    FILE * f;  // inputFile
    FILE * gf; // resultFile
    FILE * hf; // rootsFile
    int isc, id;
    int counter=0;
    float meanErr = 0;
    for(i=0;i<MAXITER;i++)
    {
        f = fopen(inputFile, "r");
        gf = fopen(resultFile, "w");
        fprintf(gf, "# id  areaEllipse1(pi.A.B)  areaEllipse2(pi.A.B)  OverlapAreaAnalytical  OverlapAreaPolynomial  err\n");
        hf = fopen(rootsFile, "w");
        if(f == NULL)
        {
            printf(">>>>>>>>> can not open file %s <<<<<<<<<<<\n", inputFile);
            exit(0);
        }
        if(gf == NULL)
        {
            printf(">>>>>>>>> can not open file %s <<<<<<<<<<<\n", resultFile);
            exit(0);
        }       
        if(hf == NULL)
        {
            printf(">>>>>>>>> can not open file %s <<<<<<<<<<<\n", rootsFile);
            exit(0);
        }
        int convert_to_radian = 0;
        //char dummy[255];
        // the file starts with this line: #id A1 B1 H1 K1 PHI_1 A2 B2 H2 K2 PHI_2
        //fgets(dummy, 255, f); //ignore first line. depends on file      
        fprintf(hf, "#roots: id x0 \ty0 \tx1 \ty1  x2 \ty2 ...\n");
        //fprintf(gf, "#no\t area_1\t\t area_2\t\t area_ellipse\t area_polygon\t err\n");
       
        while (1){
            isc = fscanf(f,"%d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",&id, &A1, &B1, &H1, &K1, &PHI_1, &A2, &B2, &H2, &K2, &PHI_2); 
            
            if(isc != 11){
                // printf("isc=%d\n",isc);
                break;
            }
            //printf("id=%d,  A1=%f,  B1=%f,  H1=%f,  K1=%f,  PHI_1=%f,  A2=%f,  B2=%f,  H2=%f,  K2=%f,  PHI_2=%f\n",id, A1, B1, H1, K1, PHI_1, A2, B2, H2, K2, PHI_2);
            /////////////////////////////////////////////////////////////////////////
            if(convert_to_radian)
            {
                PHI_1 = PHI_1 *180.0/pi;
                PHI_2 = PHI_2 *180.0/pi;
            }
            t_se = get_time();
            double x[4], y[4];
            int nroots;
          
            area = ellipse_ellipse_overlap (PHI_1, A1, B1, H1, K1,
                                            PHI_2, A2, B2, H2, K2, x, y, &nroots, &rtn);
          
            t_fe = get_time();
            time_e += (t_fe - t_se)*1000;
            ellipse2poly(PHI_1, A1, B1, H1, K1, &poly);
            ellipse2poly(PHI_2, A2, B2, H2, K2, &poly2);
            t_sp = get_time();

            areaPoly = getOverlapingAreaPoly(poly, poly2);
            t_fp = get_time();
            time_p += (t_fp - t_sp)*1000;
                    
            err = (fabs(areaPoly)<eps)?0.0:fabs(areaPoly-area)/areaPoly;
            meanErr += err;
            counter++;
            //Re = area/(pi*A2*B2 + pi*A1*B1 - area);
            //Rp = areaPoly/(pi*A2*B2 + pi*A1*B1 - areaPoly);
                    
            //Oe = area/(MIN(pi*A2*B2,pi*A1*B1));
            //Op = areaPoly/(MIN(pi*A2*B2,pi*A1*B1));

            // output roots
            fprintf(hf, " %d ",id);
            for(i=0; i<nroots; i++)
                fprintf(hf, "  %f  %f  ", x[i], y[i]);
            fprintf(hf, "\n");

            //output results
            fprintf(gf, "%d    %10.4f    %10.4f    %10.4f    %10.4f    %10.4f\n", id, pi*A1*B1, pi*A2*B2, area, areaPoly, err);
        
            //printf ("Case %d: area = %15.8f, return_value = %d, PolyArea = %15.8f (rel_err=%f)\n", id, area, rtn, areaPoly, err);
            //printf("=====================================================================================\n");
                    
            ////////////////////////////////////////////////////////////////////////
                    
        }//while
		
	fclose(f);
        fclose(gf);
        fclose(hf);
    }//MAXITER
    printf("\nrun time: %d cases. Analytical solution = %8.4lf [ms] |  Polygon approximation = %8.4lf [ms]\n", counter, time_e, time_p);
    printf("run python plot.py %s  %s to plot the results\n", inputFile, resultFile);
    fprintf(stderr, "%d  %f %f %f\n", n, time_e, time_p, meanErr/counter);
    return rtn; 
}










