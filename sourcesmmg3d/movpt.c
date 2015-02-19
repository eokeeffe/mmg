#include "mmg3d.h"


/** Move internal point */
int movintpt(pMesh mesh,int *list,int ilist,int improve) {
  pTetra               pt,pt0;
  pPoint               p0,p1,p2,p3,ppt0;
  double               vol,totvol;
  double               calold,calnew,callist[ilist];
  int                  k,iel,i0;

  pt0    = &mesh->tetra[0];
  ppt0 = &mesh->point[0];
  ppt0->c[0] = ppt0->c[1] = ppt0->c[2] = 0.0;

  /* Coordinates of optimal point */
  calold = DBL_MAX;
  totvol = 0.0;
  for (k=0; k<ilist; k++) {
    iel = list[k] / 4;
    pt = &mesh->tetra[iel];
    p0 = &mesh->point[pt->v[0]];
    p1 = &mesh->point[pt->v[1]];
    p2 = &mesh->point[pt->v[2]];
    p3 = &mesh->point[pt->v[3]];
    vol= det4pt(p0->c,p1->c,p2->c,p3->c);
    totvol += vol;
    /* barycenter */
    ppt0->c[0] += 0.25 * vol*(p0->c[0] + p1->c[0] + p2->c[0] + p3->c[0]);
    ppt0->c[1] += 0.25 * vol*(p0->c[1] + p1->c[1] + p2->c[1] + p3->c[1]);
    ppt0->c[2] += 0.25 * vol*(p0->c[2] + p1->c[2] + p2->c[2] + p3->c[2]);
    calold = MG_MIN(calold, pt->qual);
  }
  if ( totvol < EPSD2 )  return(0);
  totvol = 1.0 / totvol;
  ppt0->c[0] *= totvol;
  ppt0->c[1] *= totvol;
  ppt0->c[2] *= totvol;

  /* Check new position validity */
  calnew = DBL_MAX;
  for (k=0; k<ilist; k++) {
    iel = list[k] / 4;
    i0  = list[k] % 4;
    pt  = &mesh->tetra[iel];
    memcpy(pt0,pt,sizeof(Tetra));
    pt0->v[i0] = 0;
    callist[k] = orcal(mesh,0);
    if ( callist[k] < EPSD2 )        return(0);
    calnew = MG_MIN(calnew,callist[k]);
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if (calnew < NULKAL) return(0);
  else if ( improve && calnew < 0.9 * calold )     return(0);
  else if ( calnew < 0.3 * calold )     return(0);

  /* update position */
  p0 = &mesh->point[pt->v[i0]];
  p0->c[0] = ppt0->c[0];
  p0->c[1] = ppt0->c[1];
  p0->c[2] = ppt0->c[2];
  for (k=0; k<ilist; k++) {
    (&mesh->tetra[list[k]/4])->qual=callist[k];
  }

  return(1);
}

/** Move boundary regular point, whose volumic and surfacic balls are passed */
int movbdyregpt(pMesh mesh,int *listv,int ilistv,int *lists,int ilists) {
  pTetra                pt,pt0;
  pxTetra               pxt;
  pPoint                p0,p1,p2,ppt0;
  Tria                  tt;
  pxPoint               pxp;
  Bezier                b;
  double                *n,r[3][3],lispoi[3*LMAX+1],ux,uy,uz,det2d,detloc,oppt[2],step,lambda[3];
  double                ll,m[2],uv[2],o[3],no[3],to[3];
  double                calold,calnew,caltmp,callist[ilistv];
  int                   k,kel,iel,l,n0,na,nb,ntempa,ntempb,ntempc,nut,nxp;
  unsigned char         i0,iface,i;

  step = 0.1;
  nut    = 0;
  oppt[0] = 0.0;
  oppt[1] = 0.0;
  if ( ilists < 2 )      return(0);

  k      = listv[0] / 4;
  i0 = listv[0] % 4;
  pt = &mesh->tetra[k];
  n0 = pt->v[i0];
  p0 = &mesh->point[n0];
  assert( p0->xp && !MG_EDG(p0->tag) );

  n = &(mesh->xpoint[p0->xp].n1[0]);

  /** Step 1 : rotation matrix that sends normal n to the third coordinate vector of R^3 */
  rotmatrix(n,r);

  /** Step 2 : rotation of the oriented surfacic ball with r : lispoi[k] is the common edge
      between faces lists[k-1] and lists[k] */
  k                     = lists[0] / 4;
  iface = lists[0] % 4;
  pt            = &mesh->tetra[k];
  na = nb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != n0 ) {
      if ( !na )
        na = pt->v[idir[iface][i]];
      else
        nb = pt->v[idir[iface][i]];
    }
  }

  for (l=1; l<ilists; l++) {
    k                   = lists[l] / 4;
    iface = lists[l] % 4;
    pt          = &mesh->tetra[k];
    ntempa = ntempb = 0;
    for (i=0; i<3; i++) {
      if ( pt->v[idir[iface][i]] != n0 ) {
        if ( !ntempa )
          ntempa = pt->v[idir[iface][i]];
        else
          ntempb = pt->v[idir[iface][i]];
      }
    }
    if ( ntempa == na )
      p1 = &mesh->point[na];
    else if ( ntempa == nb )
      p1 = &mesh->point[nb];
    else if ( ntempb == na )
      p1 = &mesh->point[na];
    else {
      assert(ntempb == nb);
      p1 = &mesh->point[nb];
    }
    ux = p1->c[0] - p0->c[0];
    uy = p1->c[1] - p0->c[1];
    uz = p1->c[2] - p0->c[2];

    lispoi[3*l+1] =      r[0][0]*ux + r[0][1]*uy + r[0][2]*uz;
    lispoi[3*l+2] =      r[1][0]*ux + r[1][1]*uy + r[1][2]*uz;
    lispoi[3*l+3] =      r[2][0]*ux + r[2][1]*uy + r[2][2]*uz;

    na = ntempa;
    nb = ntempb;
  }

  /* Finish with point 0 */;
  k      = lists[0] / 4;
  iface  = lists[0] % 4;
  pt     = &mesh->tetra[k];
  ntempa = ntempb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != n0 ) {
      if ( !ntempa )
        ntempa = pt->v[idir[iface][i]];
      else
        ntempb = pt->v[idir[iface][i]];
    }
  }
  if ( ntempa == na )
    p1 = &mesh->point[na];
  else if ( ntempa == nb )
    p1 = &mesh->point[nb];
  else if ( ntempb == na )
    p1 = &mesh->point[na];
  else {
    assert(ntempb == nb);
    p1 = &mesh->point[nb];
  }

  ux = p1->c[0] - p0->c[0];
  uy = p1->c[1] - p0->c[1];
  uz = p1->c[2] - p0->c[2];

  lispoi[1] =    r[0][0]*ux + r[0][1]*uy + r[0][2]*uz;
  lispoi[2] =    r[1][0]*ux + r[1][1]*uy + r[1][2]*uz;
  lispoi[3] =    r[2][0]*ux + r[2][1]*uy + r[2][2]*uz;

  /* list goes modulo ilist */
  lispoi[3*ilists+1] = lispoi[1];
  lispoi[3*ilists+2] = lispoi[2];
  lispoi[3*ilists+3] = lispoi[3];

  /* At this point, lispoi contains the oriented surface ball of point p0, that has been rotated
     through r, with the convention that triangle l has edges lispoi[l]; lispoi[l+1] */

  /* Check all projections over tangent plane. */
  for (k=0; k<ilists-1; k++) {
    det2d = lispoi[3*k+1]*lispoi[3*(k+1)+2] - lispoi[3*k+2]*lispoi[3*(k+1)+1];
    if ( det2d < 0.0 )  return(0);
  }
  det2d = lispoi[3*(ilists-1)+1]*lispoi[3*0+2] - lispoi[3*(ilists-1)+2]*lispoi[3*0+1];
  if ( det2d < 0.0 )    return(0);

  /** Step 3 : Compute optimal position to make current triangle equilateral, and average of
      these positions*/
  for (k=0; k<ilists; k++) {
    m[0] = 0.5*(lispoi[3*(k+1)+1] + lispoi[3*k+1]);
    m[1] = 0.5*(lispoi[3*(k+1)+2] + lispoi[3*k+2]);
    ux = lispoi[3*(k+1)+1] - lispoi[3*k+1];
    uy = lispoi[3*(k+1)+2] - lispoi[3*k+2];
    ll = ux*ux + uy*uy;
    if ( ll < EPSD )    continue;
    nut++;
    oppt[0] += (m[0]-SQR32*uy);
    oppt[1] += (m[1]+SQR32*ux);
  }
  oppt[0] *= (1.0 / nut);
  oppt[1] *= (1.0 / nut);

  /** Step 4 : locate new point in the ball, and compute its barycentric coordinates */
  det2d = lispoi[1]*oppt[1] - lispoi[2]*oppt[0];
  kel = 0;
  if ( det2d >= 0.0 ) {
    for (k=0; k<ilists; k++) {
      detloc = oppt[0]*lispoi[3*(k+1)+2] - oppt[1]*lispoi[3*(k+1)+1];
      if ( detloc >= 0.0 ) {
        kel = k;
        break;
      }
    }
    if ( k == ilists ) return(0);
  }
  else {
    for (k=ilists-1; k>=0; k--) {
      detloc = lispoi[3*k+1]*oppt[1] - lispoi[3*k+2]*oppt[0];
      if ( detloc >= 0.0 ) {
        kel = k;
        break;
      }
    }
    if ( k == -1 ) return(0);
  }

  /* Sizing of time step : make sure point does not go out corresponding triangle. */
  det2d = -oppt[1]*(lispoi[3*(kel+1)+1] - lispoi[3*(kel)+1]) + \
    oppt[0]*(lispoi[3*(kel+1)+2] - lispoi[3*(kel)+2]);
  if ( fabs(det2d) < EPSD ) return(0);

  det2d = 1.0 / det2d;
  step *= det2d;

  det2d = lispoi[3*(kel)+1]*(lispoi[3*(kel+1)+2] - lispoi[3*(kel)+2]) - \
    lispoi[3*(kel)+2 ]*(lispoi[3*(kel+1)+1] - lispoi[3*(kel)+1]);
  step *= det2d;
  step  = fabs(step);
  oppt[0] *= step;
  oppt[1] *= step;

  /* Computation of the barycentric coordinates of the new point in the corresponding triangle. */
  det2d = lispoi[3*kel+1]*lispoi[3*(kel+1)+2] - lispoi[3*kel+2]*lispoi[3*(kel+1)+1];
  if ( det2d < EPSD )    return(0);
  det2d = 1.0 / det2d;
  lambda[1] = lispoi[3*(kel+1)+2]*oppt[0] - lispoi[3*(kel+1)+1]*oppt[1];
  lambda[2] = -lispoi[3*(kel)+2]*oppt[0] + lispoi[3*(kel)+1]*oppt[1];
  lambda[1]*= (det2d);
  lambda[2]*= (det2d);
  lambda[0] = 1.0 - lambda[1] - lambda[2];

  /** Step 5 : come back to original problem, and compute patch in triangle iel */
  iel    = lists[kel] / 4;
  iface  = lists[kel] % 4;
  pt     = &mesh->tetra[iel];
  pxt    = &mesh->xtetra[pt->xt];

  tet2tri(mesh,iel,iface,&tt);

  if(!bezierCP(mesh,&tt,&b,MG_GET(pxt->ori,iface))){
    fprintf(stdout,"%s:%d: Error: function bezierCP return 0\n",
            __FILE__,__LINE__);
    exit(EXIT_FAILURE);
  }

  /* Now, for Bezier interpolation, one should identify which of i,i1,i2 is 0,1,2
     recall uv[0] = barycentric coord associated to pt->v[1], uv[1] associated to pt->v[2] and
     1 - uv[0] - uv[1] is associated to pt->v[0]. For this, use the fact that kel, kel + 1 is
     positively oriented with respect to n */
  na = nb = 0;
  for( i=0 ; i<4 ; i++ ){
    if ( (pt->v[i] != n0) && (pt->v[i] != pt->v[iface]) ) {
      if ( !na )
        na = pt->v[i];
      else
        nb = pt->v[i];
    }
  }
  p1 = &mesh->point[na];
  p2 = &mesh->point[nb];
  detloc = det3pt1vec(p0->c,p1->c,p2->c,n);

  /* ntempa = point to which is associated 1 -uv[0] - uv[1], ntempb = uv[0], ntempc = uv[1] */
  ntempa = pt->v[idir[iface][0]];
  ntempb = pt->v[idir[iface][1]];
  ntempc = pt->v[idir[iface][2]];

  /* na = lispoi[kel] -> lambda[1], nb = lispoi[kel+1] -> lambda[2] */
  if ( detloc > 0.0 ) {
    if ( ntempb == na )
      uv[0] = lambda[1];
    else if (ntempb == nb )
      uv[0] = lambda[2];
    else {
      assert(ntempb == n0);
      uv[0] = lambda[0];
    }
    if ( ntempc == na )
      uv[1] = lambda[1];
    else if (ntempc == nb )
      uv[1] = lambda[2];
    else {
      assert(ntempc == n0);
      uv[1] = lambda[0];
    }
  }
  /* nb = lispoi[kel] -> lambda[1], na = lispoi[kel+1] -> lambda[2] */
  else {
    if ( ntempb == na )
      uv[0] = lambda[2];
    else if (ntempb == nb )
      uv[0] = lambda[1];
    else {
      assert(ntempb == n0);
      uv[0] = lambda[0];
    }
    if ( ntempc == na )
      uv[1] = lambda[2];
    else if (ntempc == nb )
      uv[1] = lambda[1];
    else {
      assert(ntempc == n0);
      uv[1] = lambda[0];
    }
  }
  if(!bezierInt(&b,uv,o,no,to)){
    fprintf(stdout,"%s:%d: Error: function bezierInt return 0\n",
            __FILE__,__LINE__);
    exit(EXIT_FAILURE);
  }

  /* Test : make sure that geometric approximation has not been degraded too much */
  ppt0 = &mesh->point[0];
  ppt0->c[0] = o[0];
  ppt0->c[1] = o[1];
  ppt0->c[2] = o[2];

  ppt0->tag      = p0->tag;
  ppt0->ref      = p0->ref;


  nxp = mesh->xp + 1;
  if ( nxp > mesh->xpmax ) {
    TAB_RECALLOC(mesh,mesh->xpoint,mesh->xpmax,0.2,xPoint,
                 "larger xpoint table",
                 return(0));
    n = &(mesh->xpoint[p0->xp].n1[0]);
  }
  ppt0->xp = nxp;
  pxp = &mesh->xpoint[nxp];
  memcpy(pxp,&(mesh->xpoint[p0->xp]),sizeof(xPoint));
  pxp->n1[0] = no[0];
  pxp->n1[1] = no[1];
  pxp->n1[2] = no[2];

  /* For each surfacic triangle, build a virtual displaced triangle for check purposes */
  calold = calnew = DBL_MAX;
  for (l=0; l<ilists; l++) {
    k                   = lists[l] / 4;
    iface = lists[l] % 4;
    pt          = &mesh->tetra[k];
    tet2tri(mesh,k,iface,&tt);
    calold = MG_MIN(calold,caltri(mesh,&tt));
    for( i=0 ; i<3 ; i++ )
      if ( tt.v[i] == n0 )      break;
    assert(i<3);
    tt.v[i] = 0;
    caltmp = caltri(mesh,&tt);
    if ( caltmp < EPSD )        return(0.0);
    calnew = MG_MIN(calnew,caltmp);
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if (calnew < NULKAL) return(0);
  else if ( calnew < 0.3*calold )        return(0);
  memset(pxp,0,sizeof(xPoint));

  /* Test : check whether all volumes remain positive with new position of the point */
  calold = calnew = DBL_MAX;
  for (l=0; l<ilistv; l++) {
    k    = listv[l] / 4;
    i0 = listv[l] % 4;
    pt = &mesh->tetra[k];
    pt0 = &mesh->tetra[0];
    memcpy(pt0,pt,sizeof(Tetra));
    pt0->v[i0] = 0;
    calold = MG_MIN(calold, pt->qual);
    callist[l]=orcal(mesh,0);
    if ( callist[l] < EPSD )        return(0);
    calnew = MG_MIN(calnew,callist[l]);
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if (calnew < NULKAL) return(0);
  else if ( calnew < 0.3*calold )        return(0);

  /* When all tests have been carried out, update coordinates and normals */
  p0->c[0] = o[0];
  p0->c[1] = o[1];
  p0->c[2] = o[2];

  n[0] = no[0];
  n[1] = no[1];
  n[2] = no[2];

  for(l=0; l<ilistv; l++){
    (&mesh->tetra[listv[l]/4])->qual= callist[l];
  }
  return(1);
}

/** Move boundary reference point, whose volumic and surfacic balls are passed */
int movbdyrefpt(pMesh mesh, int *listv, int ilistv, int *lists, int ilists){
  pTetra                pt,pt0;
  pxTetra               pxt;
  pPoint                p0,p1,p2,ppt0;
  Tria                  tt;
  pxPoint               pxp;
  double                step,ll1old,ll2old,o[3],no[3],to[3];
  double                calold,calnew,caltmp,callist[ilistv];
  int                   l,iel,ip0,ipa,ipb,iptmpa,iptmpb,it1,it2,ip1,ip2,ip,nxp;
  unsigned char         i,i0,ie,iface,iface1,iface2,iea,ieb,ie1,ie2;
  char                  tag;

  step = 0.1;
  ip1 = ip2 = 0;
  pt    = &mesh->tetra[listv[0]/4];
  ip0 = pt->v[listv[0]%4];
  p0    = &mesh->point[ip0];

  assert ( MG_REF & p0->tag );

  /* Travel surfacic ball and recover the two ending points of ref curve :
     two senses must be used */
  iel = lists[0]/4;
  iface = lists[0]%4;
  pt = &mesh->tetra[iel];
  ipa = ipb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != ip0 ) {
      if ( !ipa )
        ipa = pt->v[idir[iface][i]];
      else
        ipb = pt->v[idir[iface][i]];
    }
  }
  assert(ipa && ipb);

  for (l=1; l<ilists; l++) {
    iel = lists[l]/4;
    iface = lists[l]%4;
    pt = &mesh->tetra[iel];
    iea = ieb = 0;
    for (i=0; i<3; i++) {
      ie = iarf[iface][i]; //edge i on face iface
      if ( (pt->v[iare[ie][0]] == ip0) || (pt->v[iare[ie][1]] == ip0) ) {
        if ( !iea )
          iea = ie;
        else
          ieb = ie;
      }
    }
    if ( pt->v[iare[iea][0]] != ip0 )
      iptmpa = pt->v[iare[iea][0]];
    else {
      assert(pt->v[iare[iea][1]] != ip0);
      iptmpa = pt->v[iare[iea][1]];
    }
    if ( pt->v[iare[ieb][0]] != ip0 )
      iptmpb = pt->v[iare[ieb][0]];
    else {
      assert(pt->v[iare[ieb][1]] != ip0);
      iptmpb = pt->v[iare[ieb][1]];
    }
    if ( (iptmpa == ipa) || (iptmpa == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[iea];
      else  tag = 0;
      if ( MG_REF & tag ) {
        it1 = iel;
        ip1 = iptmpa;
        ie1 = iea;
        iface1 = iface;
        break;
      }
    }
    if ( (iptmpb == ipa) || (iptmpb == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[ieb];
      else  tag = 0;
      if ( MG_REF & tag ) {
        it1 = iel;
        ip1 = iptmpb;
        ie1 = ieb;
        iface1 = iface;
        break;
      }
    }
    ipa = iptmpa;
    ipb = iptmpb;
  }

  /* Now travel surfacic list in the reverse sense so as to get the second ridge */
  iel = lists[0]/4;
  iface = lists[0]%4;
  pt = &mesh->tetra[iel];
  ipa = ipb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != ip0 ) {
      if ( !ipa )
        ipa = pt->v[idir[iface][i]];
      else
        ipb = pt->v[idir[iface][i]];
    }
  }
  assert(ipa && ipb);

  for (l=ilists-1; l>0; l--) {
    iel         = lists[l] / 4;
    iface = lists[l] % 4;
    pt          = &mesh->tetra[iel];
    iea         = ieb = 0;
    for (i=0; i<3; i++) {
      ie = iarf[iface][i]; //edge i on face iface
      if ( (pt->v[iare[ie][0]] == ip0) || (pt->v[iare[ie][1]] == ip0) ) {
        if ( !iea )
          iea = ie;
        else
          ieb = ie;
      }
    }
    if ( pt->v[iare[iea][0]] != ip0 )
      iptmpa = pt->v[iare[iea][0]];
    else {
      assert(pt->v[iare[iea][1]] != ip0);
      iptmpa = pt->v[iare[iea][1]];
    }
    if ( pt->v[iare[ieb][0]] != ip0 )
      iptmpb = pt->v[iare[ieb][0]];
    else {
      assert(pt->v[iare[ieb][1]] != ip0);
      iptmpb = pt->v[iare[ieb][1]];
    }
    if ( (iptmpa == ipa) || (iptmpa == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[iea];
      else  tag = 0;
      if ( MG_REF & tag ) {
        it2 = iel;
        ip2 = iptmpa;
        ie2 = iea;
        iface2 = iface;
        break;
      }
    }
    if ( (iptmpb == ipa) || (iptmpb == ipb) ) {
      assert(pt->xt);
      tag = mesh->xtetra[pt->xt].tag[ieb];
      if ( MG_REF & tag ) {
        it2 = iel;
        ip2 = iptmpb;
        ie2 = ieb;
        iface2 = iface;
        break;
      }
    }
    ipa = iptmpa;
    ipb = iptmpb;
  }
  if ( !(ip1 && ip2 && (ip1 != ip2)) )  return(0);

  /* At this point, we get the point extremities of the ref limit curve passing through ip0 :
     ip1, ip2, along with support tets it1,it2, the surface faces iface1,iface2, and the
     associated edges ie1,ie2.*/

  /* Changes needed for choice of time step : see manuscript notes */
  p1 = &mesh->point[ip1];
  p2 = &mesh->point[ip2];

  ll1old = (p1->c[0] -p0->c[0])* (p1->c[0] -p0->c[0]) \
    + (p1->c[1] -p0->c[1])* (p1->c[1] -p0->c[1])      \
    + (p1->c[2] -p0->c[2])* (p1->c[2] -p0->c[2]);
  ll2old = (p2->c[0] -p0->c[0])* (p2->c[0] -p0->c[0]) \
    + (p2->c[1] -p0->c[1])* (p2->c[1] -p0->c[1])      \
    + (p2->c[2] -p0->c[2])* (p2->c[2] -p0->c[2]);

  if ( ll1old < ll2old ) { //move towards p2
    iel = it2;
    ie  = ie2;
    iface = iface2;
    ip = ip2;
  }
  else {
    iel = it1;
    ie  = ie1;
    iface = iface1;
    ip = ip1;
  }

  /* Compute support of the associated edge, and features of the new position */
  if ( !(BezierRef(mesh,ip0,ip,step,o,no,to)) )  return(0);

  /* Test : make sure that geometric approximation has not been degraded too much */
  ppt0 = &mesh->point[0];
  ppt0->c[0] = o[0];
  ppt0->c[1] = o[1];
  ppt0->c[2] = o[2];
  ppt0->tag  = p0->tag;
  ppt0->ref  = p0->ref;


  nxp = mesh->xp + 1;
  if ( nxp > mesh->xpmax ) {
    TAB_RECALLOC(mesh,mesh->xpoint,mesh->xpmax,0.2,xPoint,
                 "larger xpoint table",
                 return(0));
  }
  ppt0->xp = nxp;
  pxp = &mesh->xpoint[nxp];
  memcpy(pxp,&(mesh->xpoint[p0->xp]),sizeof(xPoint));

  pxp->t[0] = to[0];
  pxp->t[1] = to[1];
  pxp->t[2] = to[2];

  pxp->n1[0] = no[0];
  pxp->n1[1] = no[1];
  pxp->n1[2] = no[2];

  /* For each surface triangle, build a virtual displaced triangle for check purposes */
  calold = calnew = DBL_MAX;
  for( l=0 ; l<ilists ; l++ ){
    iel         = lists[l] / 4;
    iface       = lists[l] % 4;
    pt          = &mesh->tetra[iel];
    pxt         = &mesh->xtetra[pt->xt];
    tet2tri(mesh,iel,iface,&tt);
    calold = MG_MIN(calold,caltri(mesh,&tt));
    for( i=0 ; i<3 ; i++ )
      if ( tt.v[i] == ip0 )      break;
    assert(i<3);
    tt.v[i] = 0;
    caltmp = caltri(mesh,&tt);
    if ( caltmp < EPSD )        return(0);
    calnew = MG_MIN(calnew,caltmp);
    if ( chkedg(mesh,&tt,MG_GET(pxt->ori,iface)) ) {
      // Algiane: 09/12/2013 commit: we break the hausdorff criteria so we dont want
      // the point to move? (modification not tested because I could not find a case
      // passing here)
      memset(pxp,0,sizeof(xPoint));
      return(0);
    }
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if ( calnew < calold )    return(0);
  memset(pxp,0,sizeof(xPoint));

  /* Test : check whether all volumes remain positive with new position of the point */
  calold = calnew = DBL_MAX;
  for( l=0 ; l<ilistv ; l++ ){
    iel = listv[l] / 4;
    i0  = listv[l] % 4;
    pt  = &mesh->tetra[iel];
    pt0 = &mesh->tetra[0];
    memcpy(pt0,pt,sizeof(Tetra));
    pt0->v[i0] = 0;
    calold = MG_MIN(calold, pt->qual);
    callist[l] = orcal(mesh,0);
    if ( callist[l] < EPSD )        return(0);
    calnew = MG_MIN(calnew,callist[l]);
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if (calnew < NULKAL) return(0);
  else if ( calnew <= 0.3*calold )      return(0);

  /* Update coordinates, normals, for new point */
  p0->c[0] = o[0];
  p0->c[1] = o[1];
  p0->c[2] = o[2];

  pxp = &mesh->xpoint[p0->xp];
  pxp->n1[0] = no[0];
  pxp->n1[1] = no[1];
  pxp->n1[2] = no[2];

  pxp->t[0] = to[0];
  pxp->t[1] = to[1];
  pxp->t[2] = to[2];

  for( l=0 ; l<ilistv ; l++ ){
    (&mesh->tetra[listv[l]/4])->qual = callist[l];
  }
  return(1);
}


/** Move boundary non manifold point, whose volumic and (exterior) surfacic balls are passed */
int movbdynompt(pMesh mesh, int *listv, int ilistv, int *lists, int ilists){
  pTetra       pt,pt0;
  pxTetra      pxt;
  pPoint       p0,p1,p2,ppt0;
  pxPoint      pxp;
  Tria         tt;
  double       step,ll1old,ll2old,calold,calnew,caltmp,callist[ilistv];
  double       o[3],no[3],to[3];
  int          ip0,ip1,ip2,ip,iel,ipa,ipb,l,iptmpa,iptmpb,it1,it2,nxp;
  char         iface,i,i0,iea,ieb,ie,tag,ie1,ie2,iface1,iface2;

  step = 0.1;
  ip1 = ip2 = 0;
  pt = &mesh->tetra[listv[0]/4];
  ip0 = pt->v[listv[0]%4];
  p0 = &mesh->point[ip0];

  assert ( p0->tag & MG_NOM );

  /* Travel surfacic ball and recover the two ending points of non manifold curve :
     two senses must be used */
  iel = lists[0]/4;
  iface = lists[0]%4;
  pt = &mesh->tetra[iel];
  ipa = ipb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != ip0 ) {
      if ( !ipa )
        ipa = pt->v[idir[iface][i]];
      else
        ipb = pt->v[idir[iface][i]];
    }
  }
  assert(ipa && ipb);

  for (l=1; l<ilists; l++) {
    iel = lists[l]/4;
    iface = lists[l]%4;
    pt = &mesh->tetra[iel];
    iea = ieb = 0;
    for (i=0; i<3; i++) {
      ie = iarf[iface][i]; //edge i on face iface
      if ( (pt->v[iare[ie][0]] == ip0) || (pt->v[iare[ie][1]] == ip0) ) {
        if ( !iea )
          iea = ie;
        else
          ieb = ie;
      }
    }
    if ( pt->v[iare[iea][0]] != ip0 )
      iptmpa = pt->v[iare[iea][0]];
    else {
      assert(pt->v[iare[iea][1]] != ip0);
      iptmpa = pt->v[iare[iea][1]];
    }
    if ( pt->v[iare[ieb][0]] != ip0 )
      iptmpb = pt->v[iare[ieb][0]];
    else {
      assert(pt->v[iare[ieb][1]] != ip0);
      iptmpb = pt->v[iare[ieb][1]];
    }
    if ( (iptmpa == ipa) || (iptmpa == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[iea];
      else  tag = 0;
      if ( MG_NOM & tag ) {
        it1 = iel;
        ip1 = iptmpa;
        ie1 = iea;
        iface1 = iface;
        break;
      }
    }
    if ( (iptmpb == ipa) || (iptmpb == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[ieb];
      else  tag = 0;
      if ( MG_NOM & tag ) {
        it1 = iel;
        ip1 = iptmpb;
        ie1 = ieb;
        iface1 = iface;
        break;
      }
    }
    ipa = iptmpa;
    ipb = iptmpb;
  }

  /* Now travel surfacic list in the reverse sense so as to get the second non manifold point */
  iel = lists[0]/4;
  iface = lists[0]%4;
  pt = &mesh->tetra[iel];
  ipa = ipb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != ip0 ) {
      if ( !ipa )
        ipa = pt->v[idir[iface][i]];
      else
        ipb = pt->v[idir[iface][i]];
    }
  }
  assert(ipa && ipb);

  for (l=ilists-1; l>0; l--) {
    iel         = lists[l] / 4;
    iface = lists[l] % 4;
    pt          = &mesh->tetra[iel];
    iea         = ieb = 0;
    for (i=0; i<3; i++) {
      ie = iarf[iface][i]; //edge i on face iface
      if ( (pt->v[iare[ie][0]] == ip0) || (pt->v[iare[ie][1]] == ip0) ) {
        if ( !iea )
          iea = ie;
        else
          ieb = ie;
      }
    }
    if ( pt->v[iare[iea][0]] != ip0 )
      iptmpa = pt->v[iare[iea][0]];
    else {
      assert(pt->v[iare[iea][1]] != ip0);
      iptmpa = pt->v[iare[iea][1]];
    }
    if ( pt->v[iare[ieb][0]] != ip0 )
      iptmpb = pt->v[iare[ieb][0]];
    else {
      assert(pt->v[iare[ieb][1]] != ip0);
      iptmpb = pt->v[iare[ieb][1]];
    }
    if ( (iptmpa == ipa) || (iptmpa == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[iea];
      else  tag = 0;
      if ( MG_NOM & tag ) {
        it2 = iel;
        ip2 = iptmpa;
        ie2 = iea;
        iface2 = iface;
        break;
      }
    }
    if ( (iptmpb == ipa) || (iptmpb == ipb) ) {
      assert(pt->xt);
      tag = mesh->xtetra[pt->xt].tag[ieb];
      if ( MG_NOM & tag ) {
        it2 = iel;
        ip2 = iptmpb;
        ie2 = ieb;
        iface2 = iface;
        break;
      }
    }
    ipa = iptmpa;
    ipb = iptmpb;
  }
  if ( !(ip1 && ip2 && (ip1 != ip2)) )  return(0);

  /* At this point, we get the point extremities of the non manifold curve passing through ip0 :
     ip1, ip2, along with support tets it1,it2, the surface faces iface1,iface2, and the
     associated edges ie1,ie2.*/

  p1 = &mesh->point[ip1];
  p2 = &mesh->point[ip2];

  ll1old = (p1->c[0] -p0->c[0])* (p1->c[0] -p0->c[0]) \
    + (p1->c[1] -p0->c[1])* (p1->c[1] -p0->c[1])      \
    + (p1->c[2] -p0->c[2])* (p1->c[2] -p0->c[2]);
  ll2old = (p2->c[0] -p0->c[0])* (p2->c[0] -p0->c[0]) \
    + (p2->c[1] -p0->c[1])* (p2->c[1] -p0->c[1])      \
    + (p2->c[2] -p0->c[2])* (p2->c[2] -p0->c[2]);

  if ( ll1old < ll2old ) { //move towards p2
    iel = it2;
    ie  = ie2;
    iface = iface2;
    ip = ip2;
  }
  else {
    iel = it1;
    ie  = ie1;
    iface = iface1;
    ip = ip1;
  }

  /* Compute support of the associated edge, and features of the new position */
  if ( !(BezierNom(mesh,ip0,ip,step,o,no,to)) )  return(0);

  /* Test : make sure that geometric approximation has not been degraded too much */
  ppt0 = &mesh->point[0];
  ppt0->c[0] = o[0];
  ppt0->c[1] = o[1];
  ppt0->c[2] = o[2];
  ppt0->tag  = p0->tag;
  ppt0->ref  = p0->ref;

  nxp = mesh->xp + 1;
  if ( nxp > mesh->xpmax ) {
    TAB_RECALLOC(mesh,mesh->xpoint,mesh->xpmax,0.2,xPoint,
                 "larger xpoint table",
                 return(0));
  }
  ppt0->xp = nxp;
  pxp = &mesh->xpoint[nxp];
  memcpy(pxp,&(mesh->xpoint[p0->xp]),sizeof(xPoint));

  pxp->t[0] = to[0];
  pxp->t[1] = to[1];
  pxp->t[2] = to[2];

  pxp->n1[0] = no[0];
  pxp->n1[1] = no[1];
  pxp->n1[2] = no[2];

  /* For each surface triangle, build a virtual displaced triangle for check purposes */
  calold = calnew = DBL_MAX;
  for( l=0 ; l<ilists ; l++ ){
    iel         = lists[l] / 4;
    iface       = lists[l] % 4;
    pt          = &mesh->tetra[iel];
    pxt         = &mesh->xtetra[pt->xt];
    tet2tri(mesh,iel,iface,&tt);
    caltmp = caltri(mesh,&tt);
    calold = MG_MIN(calold,caltmp);
    for( i=0 ; i<3 ; i++ )
      if ( tt.v[i] == ip0 )      break;
    assert(i<3);

    tt.v[i] = 0;
    caltmp = caltri(mesh,&tt);
    if ( caltmp < EPSD )        return(0);
    calnew = MG_MIN(calnew,caltmp);
    if ( chkedg(mesh,&tt,MG_GET(pxt->ori,iface)) ) {
      // Algiane: 09/12/2013 commit: we break the hausdorff criteria so we dont want
      // the point to move? (modification not tested because I could not find a case
      // passing here)
      memset(pxp,0,sizeof(xPoint));
      return(0);
    }
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if ( calnew < calold )    return(0);
  memset(pxp,0,sizeof(xPoint));

  /* Test : check whether all volumes remain positive with new position of the point */
  calold = calnew = DBL_MAX;
  for( l=0 ; l<ilistv ; l++ ){
    iel = listv[l] / 4;
    i0  = listv[l] % 4;
    pt  = &mesh->tetra[iel];
    pt0 = &mesh->tetra[0];
    memcpy(pt0,pt,sizeof(Tetra));
    pt0->v[i0] = 0;
    calold = MG_MIN(calold, pt->qual);
    callist[l]= orcal(mesh,0);
    if ( callist[l] < EPSD )        return(0);
    calnew = MG_MIN(calnew,callist[l]);
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if (calnew < NULKAL) return(0);
  else if ( calnew <= 0.3*calold )      return(0);

  /* Update coordinates, normals, for new point */
  p0->c[0] = o[0];
  p0->c[1] = o[1];
  p0->c[2] = o[2];

  pxp = &mesh->xpoint[p0->xp];
  pxp->n1[0] = no[0];
  pxp->n1[1] = no[1];
  pxp->n1[2] = no[2];

  pxp->t[0] = to[0];
  pxp->t[1] = to[1];
  pxp->t[2] = to[2];

  for(l=0; l<ilistv; l++){
    (&mesh->tetra[listv[l]/4])->qual = callist[l];
  }
  return(1);
}

/** Move boundary ridge point, whose volumic and surfacic balls are passed */
int movbdyridpt(pMesh mesh,int *listv,int ilistv,int *lists,int ilists) {
  pTetra               pt,pt0;
  pxTetra              pxt;
  pPoint               p0,p1,p2,ppt0;
  Tria                 tt;
  pxPoint              pxp;
  double               step,ll1old,ll2old,o[3],no1[3],no2[3],to[3];
  double               calold,calnew,caltmp,callist[ilistv];
  int                  l,iel,ip0,ipa,ipb,iptmpa,iptmpb,it1,it2,ip1,ip2,ip,nxp;
  unsigned char        i,i0,ie,iface,iface1,iface2,iea,ieb,ie1,ie2;
  char                 tag;

  step = 0.1;
  ip1 = ip2 = 0;
  pt    = &mesh->tetra[listv[0]/4];
  ip0 = pt->v[listv[0]%4];
  p0    = &mesh->point[ip0];

  assert ( MG_GEO & p0->tag );

  /* Travel surfacic ball an recover the two ending points of ridge : two senses must be used
     POSSIBLE OPTIMIZATION HERE : One travel only is needed */
  iel           = lists[0] / 4;
  iface         = lists[0] % 4;
  pt            = &mesh->tetra[iel];
  ipa           = ipb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != ip0 ) {
      if ( !ipa )
        ipa = pt->v[idir[iface][i]];
      else
        ipb = pt->v[idir[iface][i]];
    }
  }
  assert(ipa && ipb);

  for (l=1; l<ilists; l++) {
    iel         = lists[l] / 4;
    iface = lists[l] % 4;
    pt  = &mesh->tetra[iel];
    iea = ieb = 0;
    for (i=0; i<3; i++) {
      ie = iarf[iface][i]; //edge i on face iface
      if ( (pt->v[iare[ie][0]] == ip0) || (pt->v[iare[ie][1]] == ip0) ) {
        if ( !iea )
          iea = ie;
        else
          ieb = ie;
      }
    }
    if ( pt->v[iare[iea][0]] != ip0 )
      iptmpa = pt->v[iare[iea][0]];
    else {
      assert(pt->v[iare[iea][1]] != ip0);
      iptmpa = pt->v[iare[iea][1]];
    }
    if ( pt->v[iare[ieb][0]] != ip0 )
      iptmpb = pt->v[iare[ieb][0]];
    else {
      assert(pt->v[iare[ieb][1]] != ip0);
      iptmpb = pt->v[iare[ieb][1]];
    }
    if ( (iptmpa == ipa) || (iptmpa == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[iea];
      else  tag = 0;
      if ( MG_GEO & tag ) {
        it1 = iel;
        ip1 = iptmpa;
        ie1 = iea;
        iface1 = iface;
        break;
      }
    }
    if ( (iptmpb == ipa) || (iptmpb == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[ieb];
      else  tag = 0;
      if ( MG_GEO & tag ) {
        it1 = iel;
        ip1 = iptmpb;
        ie1 = ieb;
        iface1 = iface;
        break;
      }
    }
    ipa = iptmpa;
    ipb = iptmpb;
  }

  /* Now travel surfacic list in the reverse sense so as to get the second ridge */
  iel           = lists[0] / 4;
  iface = lists[0] % 4;
  pt    = &mesh->tetra[iel];
  ipa = ipb = 0;
  for (i=0; i<3; i++) {
    if ( pt->v[idir[iface][i]] != ip0 ) {
      if ( !ipa )
        ipa = pt->v[idir[iface][i]];
      else
        ipb = pt->v[idir[iface][i]];
    }
  }
  assert(ipa && ipb);

  for (l=ilists-1; l>0; l--) {
    iel         = lists[l]/4;
    iface = lists[l]%4;
    pt  = &mesh->tetra[iel];
    iea = ieb = 0;
    for (i=0; i<3; i++) {
      ie = iarf[iface][i]; //edge i on face iface
      if ( (pt->v[iare[ie][0]] == ip0) || (pt->v[iare[ie][1]] == ip0) ) {
        if ( !iea )
          iea = ie;
        else
          ieb = ie;
      }
    }
    if ( pt->v[iare[iea][0]] != ip0 )
      iptmpa = pt->v[iare[iea][0]];
    else {
      assert(pt->v[iare[iea][1]] != ip0);
      iptmpa = pt->v[iare[iea][1]];
    }
    if ( pt->v[iare[ieb][0]] != ip0 )
      iptmpb = pt->v[iare[ieb][0]];
    else {
      assert(pt->v[iare[ieb][1]] != ip0);
      iptmpb = pt->v[iare[ieb][1]];
    }
    if ( (iptmpa == ipa) || (iptmpa == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[iea];
      else  tag = 0;
      if ( MG_GEO & tag ) {
        it2 = iel;
        ip2 = iptmpa;
        ie2 = iea;
        iface2 = iface;
        break;
      }
    }
    if ( (iptmpb == ipa) || (iptmpb == ipb) ) {
      if ( pt->xt )  tag = mesh->xtetra[pt->xt].tag[ieb];
      else  tag = 0;
      if ( MG_GEO & tag ) {
        it2 = iel;
        ip2 = iptmpb;
        ie2 = ieb;
        iface2 = iface;
        break;
      }
    }
    ipa = iptmpa;
    ipb = iptmpb;
  }
  if ( !(ip1 && ip2 && (ip1 != ip2)) ) {
    //printf("move de %d\n",ip0);
    return(0);
  }
  //assert(ip1 && ip2 && (ip1 != ip2));

  /* At this point, we get the point extremities of the ridge curve passing through ip0 :
     ip1, ip2, along with support tets it1,it2, the surface faces iface1,iface2, and the
     associated edges ie1,ie2.*/

  /* Changes needed for choice of time step : see manuscript notes */
  p1 = &mesh->point[ip1];
  p2 = &mesh->point[ip2];

  ll1old = (p1->c[0] -p0->c[0])* (p1->c[0] -p0->c[0]) \
    + (p1->c[1] -p0->c[1])* (p1->c[1] -p0->c[1])      \
    + (p1->c[2] -p0->c[2])* (p1->c[2] -p0->c[2]);
  ll2old = (p2->c[0] -p0->c[0])* (p2->c[0] -p0->c[0]) \
    + (p2->c[1] -p0->c[1])* (p2->c[1] -p0->c[1])      \
    + (p2->c[2] -p0->c[2])* (p2->c[2] -p0->c[2]);

  if ( ll1old < ll2old ) { //move towards p2
    iel = it2;
    ie  = ie2;
    iface = iface2;
    ip = ip2;
  }
  else {
    iel = it1;
    ie  = ie1;
    iface = iface1;
    ip = ip1;
  }

  /* Compute support of the associated edge, and features of the new position */
  if ( !(BezierRidge(mesh,ip0,ip,step,o,no1,no2,to)) )  return(0);

  /* Test : make sure that geometric approximation has not been degraded too much */
  ppt0 = &mesh->point[0];
  ppt0->c[0] = o[0];
  ppt0->c[1] = o[1];
  ppt0->c[2] = o[2];
  ppt0->tag      = p0->tag;
  ppt0->ref      = p0->ref;

  nxp = mesh->xp+1;
  if ( nxp > mesh->xpmax ) {
    TAB_RECALLOC(mesh,mesh->xpoint,mesh->xpmax,0.2,xPoint,
                 "larger xpoint table",
                 return(0));
  }
  ppt0->xp = nxp;
  pxp = &mesh->xpoint[nxp];
  memcpy(pxp,&(mesh->xpoint[p0->xp]),sizeof(xPoint));

  pxp->t[0] = to[0];
  pxp->t[1] = to[1];
  pxp->t[2] = to[2];

  pxp->n1[0] = no1[0];
  pxp->n1[1] = no1[1];
  pxp->n1[2] = no1[2];

  pxp->n2[0] = no2[0];
  pxp->n2[1] = no2[1];
  pxp->n2[2] = no2[2];

  /* For each surfacic triangle, build a virtual displaced triangle for check purposes */
  calold = calnew = DBL_MAX;
  for (l=0; l<ilists; l++) {
    iel         = lists[l] / 4;
    iface       = lists[l] % 4;
    pt          = &mesh->tetra[iel];
    pxt         = &mesh->xtetra[pt->xt];
    tet2tri(mesh,iel,iface,&tt);
    calold = MG_MIN(calold,caltri(mesh,&tt));
    for (i=0; i<3; i++) {
      if ( tt.v[i] == ip0 )      break;
    }
    assert(i<3);
    tt.v[i] = 0;
    caltmp = caltri(mesh,&tt);
    if ( caltmp < EPSD )        return(0);
    calnew = MG_MIN(calnew,caltmp);
    if ( chkedg(mesh,&tt,MG_GET(pxt->ori,iface)) ) {            //MAYBE CHECKEDG ASKS STH FOR POINTS !!!!!
      // Algiane: 09/12/2013 commit: we break the hausdorff criteria so we dont want
      // the point to move? (modification not tested because I could not find a case
      // passing here)
      memset(pxp,0,sizeof(xPoint));
      return(0);
    }
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if ( calnew <= calold )  return(0);
  memset(pxp,0,sizeof(xPoint));

  /* Test : check whether all volumes remain positive with new position of the point */
  calold = calnew = DBL_MAX;
  for (l=0; l<ilistv; l++) {
    iel = listv[l] / 4;
    i0  = listv[l] % 4;
    pt  = &mesh->tetra[iel];
    pt0 = &mesh->tetra[0];
    memcpy(pt0,pt,sizeof(Tetra));
    pt0->v[i0] = 0;
    calold = MG_MIN(calold, pt->qual);
    callist[l]=orcal(mesh,0);
    if ( callist[l] < EPSD )        return(0);
    calnew = MG_MIN(calnew,callist[l]);
  }
  if ( calold < NULKAL && calnew <= calold )    return(0);
  else if (calnew < NULKAL) return(0);
  else if ( calnew <= 0.3*calold )      return(0);

  /* Update coordinates, normals, for new point */
  p0->c[0] = o[0];
  p0->c[1] = o[1];
  p0->c[2] = o[2];

  pxp = &mesh->xpoint[p0->xp];
  pxp->n1[0] = no1[0];
  pxp->n1[1] = no1[1];
  pxp->n1[2] = no1[2];

  pxp->n2[0] = no2[0];
  pxp->n2[1] = no2[1];
  pxp->n2[2] = no2[2];

  pxp->t[0] = to[0];
  pxp->t[1] = to[1];
  pxp->t[2] = to[2];

  for(l=0; l<ilistv; l++){
    (&mesh->tetra[listv[l]/4])->qual = callist[l];
  }
  return(1);
}