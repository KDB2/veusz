//    Copyright (C) 2015 Jeremy S. Sanders
//    Email: Jeremy Sanders <jeremy@jeremysanders.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
/////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstdio>
#include <cmath>
#include "objects.h"

Object::~Object()
{
}

void Object::getFragments(const Mat4& outerM, FragmentVector& v)
{
}

// Triangle
///////////

void Triangle::getFragments(const Mat4& outerM, FragmentVector& v)
{
  Fragment f;
  f.type = Fragment::FR_TRIANGLE;
  f.surfaceprop = surfaceprop.ptr();
  f.lineprop = 0;
  for(unsigned i=0; i<3; ++i)
    f.points[i] = vec4to3(outerM*vec3to4(points[i]));
  f.object = this;

  v.push_back(f);
}

// PolyLine
///////////

void PolyLine::addPoints(const ValVector& x, const ValVector& y, const ValVector& z)
{
  unsigned size = std::min(x.size(), std::min(y.size(), z.size()));
  points.reserve(points.size()+size);
  for(unsigned i=0; i<size; ++i)
    points.push_back(Vec3(x[i], y[i], z[i]));
}

void PolyLine::getFragments(const Mat4& outerM, FragmentVector& v)
{
  Fragment f;
  f.type = Fragment::FR_LINESEG;
  f.surfaceprop = 0;
  f.lineprop = lineprop.ptr();
  f.object = this;

  // iterators use many more instructions here...
  for(unsigned i=0, s=points.size(); i<s; ++i)
    {
      f.points[1] = f.points[0];
      f.points[0] = vec4to3(outerM*vec3to4(points[i]));
      f.index = i;

      if(i > 0 && (f.points[0]+f.points[1]).isfinite())
        v.push_back(f);
    }
}

// LineSegments
///////////////

LineSegments::LineSegments(const ValVector& x1, const ValVector& y1, const ValVector& z1,
                           const ValVector& x2, const ValVector& y2, const ValVector& z2,
                           const LineProp* prop)
  : Object(), lineprop(prop)
{
  unsigned size = std::min( std::min(x1.size(), std::min(y1.size(), z1.size())),
                            std::min(x2.size(), std::min(y2.size(), z2.size())) );
  points.reserve(size*2);

  for(unsigned i=0; i<size; ++i)
    {
      points.push_back(Vec3(x1[i], y1[i], z1[i]));
      points.push_back(Vec3(x2[i], y2[i], z2[i]));
    }
}

LineSegments::LineSegments(const ValVector& pts1, const ValVector& pts2,
                           const LineProp* prop)
  : Object(), lineprop(prop)
{
  unsigned size = std::min(pts1.size(), pts2.size());
  for(unsigned i=0; i<size; i+=3)
    {
      points.push_back(Vec3(pts1[i], pts1[i+1], pts1[i+2]));
      points.push_back(Vec3(pts2[i], pts2[i+1], pts2[i+2]));
    }
}

void LineSegments::getFragments(const Mat4& outerM, FragmentVector& v)
{
  Fragment f;
  f.type = Fragment::FR_LINESEG;
  f.surfaceprop = 0;
  f.lineprop = lineprop.ptr();
  f.object = this;

  for(unsigned i=0, s=points.size(); i<s; i+=2)
    {
      f.points[0] = vec4to3(outerM*vec3to4(points[i]));
      f.points[1] = vec4to3(outerM*vec3to4(points[i+1]));
      f.index = i;
      v.push_back(f);
    }
}

// Mesh
///////

// get indices into vector for coordinates in height, pos1 and pos2 directions
void Mesh::getVecIdxs(unsigned &vidx_h, unsigned &vidx_1, unsigned &vidx_2) const
{
  switch(dirn)
    {
    default:
    case X_DIRN:
      vidx_h=0; vidx_1=1; vidx_2=2; break;
    case Y_DIRN:
      vidx_h=1; vidx_1=2; vidx_2=0; break;
    case Z_DIRN:
      vidx_h=2; vidx_1=0; vidx_2=1; break;
    }
}

void Mesh::getFragments(const Mat4& outerM, FragmentVector& v)
{
  getLineFragments(outerM, v);
  getSurfaceFragments(outerM, v);
}

void Mesh::getLineFragments(const Mat4& outerM, FragmentVector& v)
{
  if(lineprop.ptr() == 0)
    return;

  unsigned vidx_h, vidx_1, vidx_2;
  getVecIdxs(vidx_h, vidx_1, vidx_2);

  Fragment fl;
  fl.type = Fragment::FR_LINESEG;
  fl.surfaceprop = 0;
  fl.lineprop = lineprop.ptr();
  fl.object = this;

  const unsigned n2 = pos2.size();
  Vec4 pt(0,0,0,1);

  for(unsigned stepindex=0; stepindex<=1; ++stepindex)
    {
      const ValVector& vec_step = stepindex==0 ? pos1 : pos2;
      const ValVector& vec_const = stepindex==0 ? pos2 : pos1;
      const unsigned vidx_step = stepindex==0 ? vidx_1 : vidx_2;
      const unsigned vidx_const = stepindex==0 ? vidx_2 : vidx_1;

      for(unsigned consti=0; consti<vec_const.size(); ++consti)
        {
          pt(vidx_const) = vec_const[consti];
          for(unsigned stepi=0; stepi<vec_step.size(); ++stepi)
            {
              double heightsval = heights[stepindex==0 ? stepi*n2+consti : consti*n2+stepi];
              pt(vidx_step) = vec_step[stepi];
              pt(vidx_h) = heightsval;

              // shuffle new to old positions and calculate new new
              fl.points[1] = fl.points[0];
              fl.points[0] = vec4to3(outerM*pt);

              if(stepi > 0 && (fl.points[0]+fl.points[1]).isfinite())
                v.push_back(fl);
              ++fl.index;
            }
        }
    }
}

void Mesh::getSurfaceFragments(const Mat4& outerM, FragmentVector& v)
{
  if(surfaceprop.ptr() == 0)
    return;

  unsigned vidx_h, vidx_1, vidx_2;
  getVecIdxs(vidx_h, vidx_1, vidx_2);

  Fragment fs;
  fs.type = Fragment::FR_TRIANGLE;
  fs.surfaceprop = surfaceprop.ptr();
  fs.lineprop = 0;
  fs.object = this;

  const unsigned n1 = pos1.size();
  const unsigned n2 = pos2.size();

  Vec4 p0, p1, p2, p3;
  p0(3) = p1(3) = p2(3) = p3(3) = 1;
  for(unsigned i1=0; (i1+1)<n1; ++i1)
    for(unsigned i2=0; (i2+1)<n2; ++i2)
      {
        // grid point coordinates
        p0(vidx_h) = heights[i1*n2+i2];
        p0(vidx_1) = pos1[i1];
        p0(vidx_2) = pos2[i2];
        p1(vidx_h) = heights[(i1+1)*n2+i2];
        p1(vidx_1) = pos1[i1+1];
        p1(vidx_2) = pos2[i2];
        p2(vidx_h) = heights[i1*n2+(i2+1)];
        p2(vidx_1) = pos1[i1];
        p2(vidx_2) = pos2[i2+1];
        p3(vidx_h) = heights[(i1+1)*n2+(i2+1)];
        p3(vidx_1) = pos1[i1+1];
        p3(vidx_2) = pos2[i2+1];

        if( p1.isfinite() && p2.isfinite() )
          {
            fs.points[1] = vec4to3(outerM*p1);
            fs.points[2] = vec4to3(outerM*p2);

            if( p0.isfinite() )
              {
                // convert to outer coordinate system
                fs.points[0] = vec4to3(outerM*p0);
                v.push_back(fs);
              }
            if( p3.isfinite() )
              {
                fs.points[0] = vec4to3(outerM*p3);
                v.push_back(fs);
              }
          }
        ++fs.index;
      }
}

// DataMesh
///////////

namespace
{
  // average ignoring nan values
  double average4(double a, double b, double c, double d)
  {
    unsigned ct=0;
    double tot=0.;
    if(std::isfinite(a)) { tot+=a; ++ct; }
    if(std::isfinite(b)) { tot+=b; ++ct; }
    if(std::isfinite(c)) { tot+=c; ++ct; }
    if(std::isfinite(d)) { tot+=d; ++ct; }
    return tot/ct;
  }

  double average2(double a, double b)
  {
    unsigned ct=0;
    double tot=0.;
    if(std::isfinite(a)) { tot+=a; ++ct; }
    if(std::isfinite(b)) { tot+=b; ++ct; }
    return tot/ct;
  }

  // keep track of which lines are drawn in the grid, so they aren't
  // drawn again. We have a grid point for each edge, and a line
  // index, stored as a bit
  struct LineCellTracker
  {
    LineCellTracker(unsigned _n1, unsigned _n2)
      : n1(_n1), n2(_n2), data(n1*n2, 0)
    {
    }

    void setLine(unsigned i1, unsigned i2, unsigned idx)
    {
      data[i1*n2+i2] |= 1<<idx;
    }

    bool isLineSet(unsigned i1, unsigned i2, unsigned idx) const
    {
      return data[i1*n2+i2] & 1<<idx;
    }

    unsigned n1, n2;
    std::vector<unsigned> data;
  };
};


void DataMesh::getFragments(const Mat4& outerM, FragmentVector& v)
{
  // check indices
  bool found[3] = {0, 0, 0};
  unsigned idxs[3] = {idxval, idxedge1, idxedge2};
  for(unsigned i=0; i<3; ++i)
    if(idxs[i]<=2)
      found[idxs[i]]=1;
  if(!found[0] || !found[1] || !found[2])
    {
      std::fprintf(stderr, "DataMesh: invalid indices\n");
      return;
    }

  // check that data sizes agree
  if( (int(edges1.size())-1)*(int(edges2.size())-1) != int(vals.size()) )
    {
      std::fprintf(stderr, "DataMesh: invalid size\n");
      return;
    }

  // nothing to draw
  if( lineprop.ptr()==0 && surfaceprop.ptr()==0 )
    return;

  // used to draw the grid and surface
  Fragment ft;
  ft.type = Fragment::FR_TRIANGLE;
  ft.surfaceprop = surfaceprop.ptr();
  ft.lineprop = 0;
  ft.object = this;

  Fragment fl;
  fl.type = Fragment::FR_LINESEG;
  fl.surfaceprop = 0;
  fl.lineprop = lineprop.ptr();
  fl.object = this;

  // these are the corner indices used for drawing low and high resolution surfaces
  static const unsigned trilist_lowres[2][3] = {{0,2,4},{0,6,4}};
  static const unsigned trilist_highres[8][3]  = {
    {8,0,1},{8,1,2},{8,2,3},{8,3,4},{8,4,5},{8,5,6},{8,6,7},{8,7,0}};
  static const unsigned linelist_lowres[4][2] = {
    {0,2},{0,6},{4,2},{4,6}};
  static const unsigned linelist_highres[8][2] = {
    {0,1},{1,2},{2,3},{3,4},{4,5},{5,6},{6,7},{7,0}};

  // this is to avoid double-drawing lines
  // xidx, yidx, lineidx
  static const unsigned linecell_lowres[4][3] = {
    {0,0,0}, {0,0,1}, {0,1,0}, {1,0,1}
  };
  static const unsigned linecell_highres[8][3] = {
    {0,0,0}, {0,0,1}, {1,0,2}, {1,0,3}, {0,1,1}, {0,1,0}, {0,0,3}, {0,0,2}
  };

  // select list above depending on high or low resolution
  const unsigned (*tris)[3] = highres ? trilist_highres : trilist_lowres;
  const unsigned (*lines)[2] = highres ? linelist_highres : linelist_lowres;
  const unsigned (*linecells)[3] = highres ? linecell_highres :
    linecell_lowres;
  const unsigned ntris = highres ? 8 : 2;
  const unsigned nlines = highres ? 8 : 4;

  // store corners and neighbouring cell values
  double neigh[9];
  Vec4 corners[9];   // 4d corners
  for(unsigned i=0; i<9; ++i)
    corners[i](3) = 1;
  Vec3 corners3[9];  // 3d version of above

  // don't draw lines twice by keeping track if which edges of which
  // cells have been drawn already
  LineCellTracker linetracker(edges1.size(), edges2.size());

  const int n1=int(edges1.size())-1;
  const int n2=int(edges2.size())-1;

  // loop over 2d array
  for(int i1=0; i1<n1; ++i1)
    for(int i2=0; i2<n2; ++i2)
      {
        // skip bad data values
        if( ! std::isfinite(vals[i1*n2+i2]) )
          continue;

        // get values of neighbouring cells (clipping at edges)
        // -1,-1 -1,0 -1,1   0,-1 0,0 0,1   1,-1 1,0 1,1
        for(int d1=-1; d1<=1; ++d1)
          for(int d2=-1; d2<=1; ++d2)
            {
              int clip1 = std::max(std::min(i1+d1, n1-1), 0);
              int clip2 = std::max(std::min(i2+d2, n2-1), 0);
              double val = vals[clip1*n2+clip2];
              neigh[(d1+1)*3+(d2+1)] = val;
            }

        // compute "corners" - these are the clockwise corners and
        // edge centres from the top left (d1==d2==-1), followed by
        // the cell centre
        corners[0](idxs[0]) = average4(neigh[0],neigh[3],neigh[4],neigh[1]);
        corners[0](idxs[1]) = edges1[i1];
        corners[0](idxs[2]) = edges2[i2];

        corners[1](idxs[0]) = average2(neigh[4],neigh[3]);
        corners[1](idxs[1]) = 0.5*(edges1[i1]+edges1[i1+1]);
        corners[1](idxs[2]) = edges2[i2];

        corners[2](idxs[0]) = average4(neigh[3],neigh[6],neigh[7],neigh[4]);
        corners[2](idxs[1]) = edges1[i1+1];
        corners[2](idxs[2]) = edges2[i2];

        corners[3](idxs[0]) = average2(neigh[4],neigh[7]);
        corners[3](idxs[1]) = edges1[i1+1];
        corners[3](idxs[2]) = 0.5*(edges2[i2]+edges2[i2+1]);

        corners[4](idxs[0]) = average4(neigh[4],neigh[7],neigh[8],neigh[5]);
        corners[4](idxs[1]) = edges1[i1+1];
        corners[4](idxs[2]) = edges2[i2+1];

        corners[5](idxs[0]) = average2(neigh[4],neigh[5]);
        corners[5](idxs[1]) = 0.5*(edges1[i1]+edges1[i1+1]);
        corners[5](idxs[2]) = edges2[i2+1];

        corners[6](idxs[0]) = average4(neigh[1],neigh[4],neigh[5],neigh[2]);
        corners[6](idxs[1]) = edges1[i1];
        corners[6](idxs[2]) = edges2[i2+1];

        corners[7](idxs[0]) = average2(neigh[4],neigh[1]);
        corners[7](idxs[1]) = edges1[i1];
        corners[7](idxs[2]) = 0.5*(edges2[i2]+edges2[i2+1]);

        corners[8](idxs[0]) = neigh[4];
        corners[8](idxs[1]) = 0.5*(edges1[i1]+edges1[i1+1]);
        corners[8](idxs[2]) = 0.5*(edges2[i2]+edges2[i2+1]);

        // convert to 3d coordinates
        for(unsigned i=0; i<9; ++i)
          corners3[i] = vec4to3(outerM*corners[i]);

        // draw triangles
        if(ft.surfaceprop!=0)
          {
            ft.index = i1*n2+i2;
            for(unsigned i=0; i<ntris; ++i)
              {
                ft.points[0] = corners3[tris[i][0]];
                ft.points[1] = corners3[tris[i][1]];
                ft.points[2] = corners3[tris[i][2]];
                v.push_back(ft);
              }
          }

        // draw lines
        if(fl.lineprop!=0)
          {
            fl.index = i1*n2+i2;
            for(unsigned i=0; i<nlines; ++i)
              {
                if(! linetracker.isLineSet(i1+linecells[i][0], i2+linecells[i][1],
                                           linecells[i][2]))
                  {
                    fl.points[0] = corners3[lines[i][0]];
                    fl.points[1] = corners3[lines[i][1]];
                    if(fl.points[0].isfinite() && fl.points[1].isfinite())
                      v.push_back(fl);
                    linetracker.setLine(i1+linecells[i][0], i2+linecells[i][1],
                                        linecells[i][2]);
                  }
              }
          }
      } // loop over points

}

// Points
/////////

void Points::getFragments(const Mat4& outerM, FragmentVector& v)
{
  fragparams.path = &path;
  fragparams.scaleedges = scaleedges;
  fragparams.runcallback = 0;

  Fragment fp;
  fp.type = Fragment::FR_PATH;
  fp.object = this;
  fp.params = &fragparams;
  fp.surfaceprop = surfacefill.ptr();
  fp.lineprop = lineedge.ptr();
  fp.pathsize = 1;

  unsigned size = std::min(x.size(), std::min(y.size(), z.size()));
  bool hassizes = !sizes.empty();
  if(hassizes)
    size = std::min(size, unsigned(sizes.size()));

  for(unsigned i=0; i<size; ++i)
    {
      fp.points[0] = vec4to3(outerM*Vec4(x[i], y[i], z[i], 1));
      if(hassizes)
        fp.pathsize = sizes[i];
      fp.index = i;

      if(fp.points[0].isfinite())
        v.push_back(fp);
    }
}


// Text
///////

Text::Text(const ValVector& _pos1, const ValVector& _pos2)
  : pos1(_pos1), pos2(_pos2)
{
  fragparams.text = this;
  fragparams.path = 0;
  fragparams.scaleedges = 0;
  fragparams.runcallback = 1;
}

void Text::TextPathParameters::callback(QPainter* painter, QPointF pt1,
                                        QPointF pt2, unsigned index,
                                        double scale, double linescale)
{
  text->draw(painter, pt1, pt2, index, scale, linescale);
}

void Text::getFragments(const Mat4& outerM, FragmentVector& v)
{
  Fragment fp;
  fp.type = Fragment::FR_PATH;
  fp.object = this;
  fp.params = &fragparams;
  fp.surfaceprop = 0;
  fp.lineprop = 0;
  fp.pathsize = 1;

  unsigned numitems = std::min(pos1.size(), pos2.size()) / 3;
  for(unsigned i=0; i<numitems; ++i)
    {
      unsigned base = i*3;
      Vec4 pt1(pos1[base], pos1[base+1], pos1[base+2]);
      fp.points[0] = vec4to3(outerM*pt1);
      Vec4 pt2(pos2[base], pos2[base+1], pos2[base+2]);
      fp.points[1] = vec4to3(outerM*pt2);
      fp.index = i;
      v.push_back(fp);
    }
}

void Text::draw(QPainter* painter, QPointF pt1, QPointF pt2,
                unsigned index, double scale, double linescale)
{
}

// TriangleFacing
/////////////////

void TriangleFacing::getFragments(const Mat4& outerM, FragmentVector& v)
{
  Vec3 torigin = vec4to3(outerM*Vec4(0,0,0,1));
  Vec3 norm = cross(points[1]-points[0], points[2]-points[0]);
  Vec3 tnorm = vec4to3(outerM*vec3to4(norm));

  // norm points towards +z
  if(tnorm(2) > torigin(2))
    Triangle::getFragments(outerM, v);
}

// ObjectContainer
//////////////////

ObjectContainer::~ObjectContainer()
{
  for(unsigned i=0, s=objects.size(); i<s; ++i)
    delete objects[i];
}


void ObjectContainer::getFragments(const Mat4& outerM, FragmentVector& v)
{
  Mat4 totM(outerM*objM);
  unsigned s=objects.size();
  for(unsigned i=0; i<s; ++i)
    objects[i]->getFragments(totM, v);
}

// FacingContainer

void FacingContainer::getFragments(const Mat4& outerM, FragmentVector& v)
{
  Vec3 origin = vec4to3(outerM*Vec4(0,0,0,1));
  Vec3 tnorm = vec4to3(outerM*vec3to4(norm));

  // norm points towards +z
  if(tnorm(2) > origin(2))
    ObjectContainer::getFragments(outerM, v);
}
