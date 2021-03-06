#include "cover_tree.hpp"
#include "vector_mult.hpp"

#include <cmath>
#include <unistd.h>

double dist_euclidean_deriv_wrt_xi(const double *p1, const double *p2, int i, double d, double BOUND_IGNORED, const double *scales, void *dims) {
  // derivative of dist_euclidean(p1,p2) with respect to the i'th element of p1.
  if (d == 0)
    return 0;

  return (p1[i] - p2[i]) / (scales[i] * scales[i] * d) ;
}

double dist_euclidean_deriv_wrt_theta(const double *p1, const double *p2, int i, double d, double BOUND_IGNORED, const double *scales, void *dims) {
  // derivative of dist_euclidean(p1,p2) with respect to the i'th lengthscale.
  if (d == 0)
    return 0;

  double diff = (p1[i] - p2[i])/scales[i];
  return - diff*diff / ( scales[i] * d);
}

double dist_euclidean(const point p1, const point p2, double BOUND_IGNORED, const double *scales, void *dims) {
  (( int *) dims)[1] += 1; // dfn_calls counter
  return sqrt(sqdist_euclidean(p1.p, p2.p, BOUND_IGNORED, scales, dims));
}

double dist_euclidean(const double * p1, const double * p2, double BOUND_IGNORED, const double *scales, const void *dims) {
  return sqrt(sqdist_euclidean(p1, p2, BOUND_IGNORED, scales, dims));
}

double sqdist_euclidean(const double * p1, const double * p2, double BOUND_IGNORED, const double *scales, const void *dims) {
  int d = *(int *)dims;
  double sqdist = 0;
  double diff = 0;
  //printf("sqdist dim %d: ", d);
  for (int i=0; i < d; ++i) {
    diff = (p1[i] - p2[i]) / scales[i];
    sqdist += (diff * diff);
    //printf("+ %f*%f ", diff, diff);
  }
  //printf(" = %f\n", sqdist);
  return sqdist;
}

double pair_dist_euclidean(const pairpoint p1, const pairpoint p2, double BOUND_IGNORED, const double *scales, void *dims) {
  double d1 = sqdist_euclidean(p1.pt1, p2.pt1, BOUND_IGNORED, scales, dims);
  double d2 = sqdist_euclidean(p1.pt2, p2.pt2, BOUND_IGNORED, scales, dims);
  return sqrt(d1 + d2);
}

static const double AVG_EARTH_RADIUS_KM = 6371.0;
static double RADIAN(double x) {return x*3.14159265/180.0;}
double dist_km(const double *p1, const double *p2) {

  double lon1 = p1[0];
  double lat1 = p1[1];
  double lon2 = p2[0];
  double lat2 = p2[1];
  double rlon1 = RADIAN(lon1);
  double rlat1 = RADIAN(lat1);
  double rlon2 = RADIAN(lon2);
  double rlat2 = RADIAN(lat2);

  /*
  double dist_rad = acos(sin(rlat1)
			* sin(rlat2)
			+ cos(rlat1)
			* cos(rlat2)
			* cos(rlon2 - rlon1));
			*/

  double dist_rad = 2*asin(sqrt(
			     pow(sin((rlat1-rlat2)/2.0),2) +
			     cos(rlat1)*cos(rlat2)*
			     pow(sin((rlon1-rlon2)/2.0),2)
			     ));

  //printf("returning C:dist_km of (%f, %f) and (%f, %f) is %f\n", lon1, lat1, lon2, lat2, dist_rad * AVG_EARTH_RADIUS_KM);
  return dist_rad * AVG_EARTH_RADIUS_KM;
}

double dist_km_deriv_wrt_xi_empirical(const double *p1, const double *p2, int i, int d) {
  double eps = 1e-8;
  double pp[3];
  pp[0] = p1[0];
  pp[1] = p1[1];
  pp[2] = p1[2];
  pp[i] += eps;
  double dkm2 = dist_km((const double *)&pp, p2);
  return (dkm2-d)/eps;
}

double dist_km_deriv_wrt_xi(const double *p1, const double *p2, int i, int d) {
  double lon1 = p1[0];
  double lat1 = p1[1];
  double lon2 = p2[0];
  double lat2 = p2[1];
  double rlon1 = RADIAN(lon1);
  double rlat1 = RADIAN(lat1);
  double rlon2 = RADIAN(lon2);
  double rlat2 = RADIAN(lat2);


    /*
      quick hack from wolfram alpha:
      "derivative of arcsin( sqrt( sin( (x-y)/2 )^2 + cos(x)*cos(y)* sin ((a-b)/2)^2)) with respect to x"

where

  x = rlat1;
  y = rlat2;
  a = rlon1;
  b= rlon2;

     */


  double t = cos(rlat1)*cos(rlat2)*pow(sin((rlon1-rlon2)/2.0),2) + pow(sin((rlat1-rlat2)/2.0),2);
  //double t2 = pow(sin(d/(2*AVG_EARTH_RADIUS_KM)), 2);
  //printf("t1 %f t2 %f\n", t, t2);
  double deriv_denom = 2 * sqrt( (1-t) * (t)  );

  double deriv_num = 0;
  if (i == 0) {
    deriv_num = cos(rlat1)*cos(rlat2)*sin(rlon1-rlon2);
  } else if (i ==1) {
    deriv_num = sin(rlat1-rlat2) - 2 * sin(rlat1)*cos(rlat2)*pow(sin((rlon1-rlon2)/2.0), 2);
  } else {
    printf("don't know how to take derivative of great-circle distance with respect to input index %d\n", i);
    exit(0);
  }

  if (deriv_denom == 0) {
    double t2 = pow(sin(d/(2*AVG_EARTH_RADIUS_KM)), 2);
    //printf("WARNING: zero denom deriv in t1 %f t2 %f d %f d/stuff %f dlon %f dlat %f num %f\n", t*1e8, t2*1e8, d*1e8, d/(2*AVG_EARTH_RADIUS_KM) * 1e8, rlon1-rlon2, rlat1-rlat2, deriv_num);
    return dist_km_deriv_wrt_xi_empirical(p1, p2, i, d);
  }

  return deriv_num/deriv_denom * 3.14159265/180.0 * AVG_EARTH_RADIUS_KM;
}


double dist_3d_km(const point p1, const point p2, double BOUND_IGNORED, const double *scales, void * extra) {
  return sqrt(distsq_3d_km(p1.p, p2.p, BOUND_IGNORED, scales, extra));
}

double dist_3d_km(const double * p1, const double * p2, double BOUND_IGNORED, const double *scales, const void * extra) {
  return sqrt(distsq_3d_km(p1, p2, BOUND_IGNORED, scales, extra));
}


double distsq_3d_km(const double * p1, const double * p2, double BOUND_IGNORED, const double *scales, const void * extra) {
  double distkm = dist_km(p1, p2) / scales[0];
  double dist_d = (p2[2] - p1[2]) / scales[1];
  //printf("dist3d returning sqrt(%f^2 + %f^2) = %f\n", distkm, dist_d, sqrt(pow(distkm, 2) + pow(dist_d, 2)));
  return pow(distkm, 2) + pow(dist_d, 2);
}



double dist_6d_km(const point p1, const point p2, double BOUND_IGNORED, const double *scales, void *extra) {
  return sqrt(distsq_6d_km(p1.p, p2.p, BOUND_IGNORED, scales, extra));
}

double dist_6d_km(const double * p1, const double * p2, double BOUND_IGNORED, const double *scales, const void *extra) {
  return sqrt(distsq_6d_km(p1, p2, BOUND_IGNORED, scales, extra));
}

double distsq_6d_km(const double * p1, const double * p2, double BOUND_IGNORED, const double *scales,const void *extra) {
  double sta_distkm = dist_km(p1, p2) / scales[0];
  double sta_dist_d = (p2[2] - p1[2]) / scales[1];
  double ev_distkm = dist_km(p1+3, p2+3) / scales[2];
  double ev_dist_d = (p2[5] - p1[5]) / scales[3];

  return pow(sta_distkm, 2) + pow(sta_dist_d, 2) + pow(ev_distkm, 2) + pow(ev_dist_d, 2);
}

double pair_dist_6d_km(const pairpoint p1, const pairpoint p2, double BOUND_IGNORED, const double *scales, void * extra) {
  double sta_distkm1 = dist_km(p1.pt1, p2.pt1) / scales[0];
  double sta_distkm2 = dist_km(p1.pt2, p2.pt2) / scales[0];
  double ev_distkm1 = dist_km(p1.pt1, p2.pt1) / scales[2];
  double ev_distkm2 = dist_km(p1.pt2, p2.pt2) / scales[2];

  double sta_dist_d1 = (p2.pt1[2] - p1.pt1[2]) / scales[1];
  double sta_dist_d2 = (p2.pt2[2] - p1.pt2[2]) / scales[1];
  double ev_dist_d1 = (p2.pt1[2] - p1.pt1[2]) / scales[1];
  double ev_dist_d2 = (p2.pt2[2] - p1.pt2[2]) / scales[1];

  return sqrt(pow(sta_distkm1, 2) + pow(sta_distkm2, 2) + pow(sta_dist_d1, 2) + pow(sta_dist_d2,2) + pow(ev_distkm1, 2) + pow(ev_distkm2, 2) + pow(ev_dist_d1, 2) + pow(ev_dist_d2, 2));
}



double pair_dist_3d_km(const pairpoint p1, const pairpoint p2, double BOUND_IGNORED, const double *scales, void  * extra) {
  double distkm1 = dist_km(p1.pt1, p2.pt1) / scales[0];
  double distkm2 = dist_km(p1.pt2, p2.pt2) / scales[0];

  double dist_d1 = (p2.pt1[2] - p1.pt1[2]) / scales[1];
  double dist_d2 = (p2.pt2[2] - p1.pt2[2]) / scales[1];
  return sqrt(pow(distkm1, 2) + pow(distkm2, 2) + pow(dist_d1, 2) + pow(dist_d2,2));
}

double dist3d_deriv_wrt_theta(const double *p1, const double *p2, int i, double d, double BOUND_IGNORED, const double *scales, void *dims) {

  if (d == 0)
    return 0;

  if (i == 0) {
    double distkm = dist_km(p1, p2) / scales[0];
    return -distkm*distkm / (scales[0] * d);
  } else if (i ==1) {
    double dist_d = (p2[2] - p1[2]) / scales[1];
    return -dist_d * dist_d / (scales[1] * d);
  } else {
    printf("taking derivative wrt unrecognized parameter %d!\n", i);
    exit(-1);
    return 0;
  }
}

double dist3d_deriv_wrt_xi(const double * p1, const double * p2, int i, double d, double BOUND_IGNORED, const double *scales, void * extra) {

  if (d == 0)
    return 0;

  if (i < 2) {
    // deriv of sqrt((dkm/scale)^2 + (ddb-dda)^2/scale2^2)
    // is   (dkm/scale) * (d_dkm_di/scale)   / d

    double dkm = dist_km(p1, p2);
    double d_dkm_di = dist_km_deriv_wrt_xi(p1, p2, i, dkm);

    /*
    double eps = 1e-8;
    double pp[3];
    pp[0] = p1[0];
    pp[1] = p1[1];
    pp[2] = p1[2];
    pp[i] += eps;
    double dkm2 = dist_km((const double *)&pp, p2);

    double empirical_dd = (dkm2-dkm)/eps;

    printf("i %d distkm %f distkm2 %f dd %f empirical dd %f\n", i, dkm, dkm2, d_dkm_di, empirical_dd);
    */

    double dxi = (dkm * d_dkm_di) / (scales[0] * scales[0] * d);

    if (dxi < -99999) {
      //printf("WARNING: invalid lld derivative!\n i %d dkm %f d_dkm_di %f scale %f d %f dxi %f p1 (%f, %f, %f) p2 (%f, %f, %f)\n", i, dkm, d_dkm_di, scales[0], d, dxi, p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
      return 0;
    }

    return dxi;
  } if (i==2) {
    return (p1[i] - p2[i]) / (scales[1] * scales[1] * d) ;
  } else {
    printf("don't know how to take derivative of great-circle+depth distance with respect to input index %d\n", i);
    exit(0);
  }

}


double dist6d_deriv_wrt_theta(const double *p1, const double *p2, int i, double d, double BOUND_IGNORED, const double *scales, void *dims) {

  if (d == 0)
    return 0;


  if (i == 0) {
    double sta_distkm = dist_km(p1, p2) / scales[0];
    return -sta_distkm*sta_distkm / (scales[0] * d);
  } else if (i ==1) {
    double sta_dist_d = (p2[2] - p1[2]) / scales[1];
    return -sta_dist_d * sta_dist_d / (scales[1] * d);
  } else if (i ==2) {
    double ev_distkm = dist_km(p1+3, p2+3) / scales[2];
    return -ev_distkm * ev_distkm / (scales[2] * d);
  } else if (i ==3) {
    double dist_d = (p2[5] - p1[5]) / scales[3];
    return -dist_d * dist_d / (scales[3] * d);
  } else {
    printf("taking derivative wrt unrecognized parameter %d!\n", i);
    exit(-1);
    return 0;
  }
}

double w_se(double d, const double * variance) {
  return variance[0] * exp(-1 * d*d);
}

double deriv_se_wrt_r(double r, double dr_dtheta, const double *variance) {
  return variance[0] * exp(-1 * r * r) * -2 * r * dr_dtheta;
}

double w_e(double d, const double * variance) {
  return variance[0] * exp(-1 * d);
}

double deriv_matern32_wrt_r(double r, double dr_dtheta, const double *variance) {
  double sqrt3 = 1.73205080757;
  return variance[0] * exp(-sqrt3 * r) * -3 * r * dr_dtheta;
}

double w_matern32(double d, const double * variance) {
  double sqrt3 = 1.73205080757;
  return variance[0] * (1 + sqrt3*d) * exp(-sqrt3 * d);
}

double w_matern32_lower(double d, const double *variance) {
  double sqrt3 = 1.73205080757;
  return variance[0] * (1 + sqrt3*d) * exp(-sqrt3 * d);
}

double w_matern32_upper(double d, const double *variance) {
  double sqrt3 = 1.73205080757;
  return variance[0] * (1 + sqrt3*d + .75*d*d) * exp(-sqrt3 * d);
}

double w_compact_q0(double r, const double * extra) {
  double d = 1.0 - r;
  if (d <= 0.0) {
    return 0.0;
  }

  // eqn 4.21 from rasmussen & williams
  double variance = extra[0];
  int j = (int)extra[1];

  return variance * pow(d, j);
}

double w_compact_q0_lower(double r, const double * extra) {
  double d = 1.0 - r;
  if (d <= 0.0) {
    return 0.0;
  }

  double variance = extra[0];
  int j = (int)extra[1];


  return variance * pow(d, j);
}

double w_compact_q0_upper(double r, const double * extra) {
  double d = 1.0 - r + .25 * r * r; // this is an upper bound for r < 2.0
  if (r >= 2.0) {
    return 0.0;
  }

  double variance = extra[0];
  int j = (int)extra[1];

  return variance * pow(d, j);
}


double w_compact_q2(double r, const double * extra) {
  double d = 1.0 - r;
  if (d <= 0.0) {
    return 0.0;
  }

  // eqn 4.21 from rasmussen & williams
  double variance = extra[0];
  int j = (int)extra[1];

  double poly = ((j*j + 4*j + 3)*r*r + (3*j + 6)*r + 3)/3.0;
  return variance * pow(d, j+2)*poly;
  }


double deriv_compact_q2_wrt_r(double r, double dr_dtheta, const double *extra) {
  double d = 1.0 - r;
  if (d <= 0.0) {
    return 0.0;
  }

  // eqn 4.21 from rasmussen & williams
  double variance = extra[0];
  int j = (int)extra[1];
  double poly = ((j*j + 4*j + 3)*r*r + (3*j + 6)*r + 3)/3.0;

  double dpoly_dr = ((2*j*j + 8*j + 6.)*r + 3*j + 6.) / 3.0;
  double dk_dr = variance * (pow(d, j+2)*dpoly_dr - (j+2)*pow(d, j+1) * poly);

  return dk_dr * dr_dtheta;
}


double w_compact_q2_lower(double r, const double * extra) {
  double d = 1.0 - r;
  if (d <= 0.0) {
    return 0.0;
  }

  double variance = extra[0];
  int j = (int)extra[1];

  double poly1 = (3*j*j + 12*j + 9) * r * r / 2.0;
  double poly2 = (9*j + 18) * r;
  double poly = (poly1 + poly2 + 9.0)/9.0;
  return variance * pow(d, j+2) * poly;
}

double w_compact_q2_upper(double r, const double * extra) {
  double d = 1.0 - r + .25 * r * r; // this is an upper bound for r < 2.0
  if (r >= 2.0) {
    return 0.0;
  }

  double variance = extra[0];
  int j = (int)extra[1];

  double rsq4 = r * r / 4.0;
  double jquad = (j*j + 4*j + 3);
  double jlinear = 3*j + 6;

  double poly1 = jquad * rsq4;
  double poly1sq = poly1 * poly1;
  double poly2 = jquad * jlinear * rsq4 * r;
  double poly3 = poly1 * 12.0;
  double poly4 = jlinear * jlinear * rsq4;
  double poly5 = jlinear * r * 3.0;
  double poly = (poly1sq + poly2 + poly3 + poly4 + poly5 + 9.0) / 9.0;

  return variance * pow(d, j+2) * poly;
}
