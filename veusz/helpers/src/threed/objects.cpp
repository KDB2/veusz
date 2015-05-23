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
#include "objects.h"

Object::~Object()
{
}

void Object::getFragments(const Mat4& outerM, const Camera& cam,
			  FragmentVector& v)
{
}

// Triangle
///////////

void Triangle::getFragments(const Mat4& outerM, const Camera& cam,
			    FragmentVector& v)
{
  Fragment f;
  f.type = Fragment::FR_TRIANGLE;
  f.surfaceprop = surfaceprop.ptr();
  f.lineprop = 0;
  Vec4 p0 = outerM*points[0];
  f.points[0] = vec4to3(p0);
  f.proj[0] = calcProjVec(cam.perspM, p0);
  Vec4 p1 = outerM*points[1];
  f.points[1] = vec4to3(p1);
  f.proj[1] = calcProjVec(cam.perspM, p1);
  Vec4 p2 = outerM*points[2];
  f.points[2] = vec4to3(p2);
  f.proj[2] = calcProjVec(cam.perspM, p2);
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
    points.push_back(Vec4(x[i], y[i], z[i], 1));
}

void PolyLine::getFragments(const Mat4& outerM, const Camera& cam,
			    FragmentVector& v)
{
  Fragment f;
  f.type = Fragment::FR_LINESEG;
  f.surfaceprop = 0;
  f.lineprop = lineprop.ptr();
  f.object = this;

  // iterators use many more instructions here...
  for(unsigned i=0; i<points.size(); ++i)
    {
      f.points[1] = f.points[0];
      f.proj[1] = f.proj[0];

      Vec4 pt = outerM*points[i];
      f.points[0] = vec4to3(pt);
      f.proj[0] = calcProjVec(cam.perspM, pt);

      if(i > 0 && (f.points[0]+f.points[1]).isfinite())
        {
          v.push_back(f);
          f.bumpIndex();
        }
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

void Mesh::getFragments(const Mat4& outerM, const Camera& cam,
                        FragmentVector& v)
{
  getLineFragments(outerM, cam, v);
  getSurfaceFragments(outerM, cam, v);
}

void Mesh::getLineFragments(const Mat4& outerM, const Camera& cam,
                            FragmentVector& v)
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
              Vec4 rpt = outerM*pt;

              // shuffle new to old positions and calculate new new
              fl.points[1] = fl.points[0];
              fl.points[0] = vec4to3(rpt);
              fl.proj[1] = fl.proj[0];
              fl.proj[0] = calcProjVec(cam.perspM, rpt);

              if(stepi > 0 && (fl.points[0]+fl.points[1]).isfinite())
                {
                  v.push_back(fl);
                  fl.bumpIndex();
                }
            }
        }
    }
}

void Mesh::getSurfaceFragments(const Mat4& outerM, const Camera& cam,
                               FragmentVector& v)
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

        if(! ((p0+p1+p2+p3).isfinite()) )
          continue;

        // these are converted to the outer coordinate system
        Vec4 rp0 = outerM*p0;
        Vec4 rp1 = outerM*p1;
        Vec4 rp2 = outerM*p2;
        Vec4 rp3 = outerM*p3;

        fs.points[1] = vec4to3(rp1);
        fs.points[2] = vec4to3(rp2);
        fs.proj[1] = calcProjVec(cam.perspM, rp1);
        fs.proj[2] = calcProjVec(cam.perspM, rp2);

        fs.points[0] = vec4to3(rp0);
        fs.proj[0] = calcProjVec(cam.perspM, rp0);
        v.push_back(fs);
        fs.bumpIndex();

        fs.points[0] = vec4to3(rp3);
        fs.proj[0] = calcProjVec(cam.perspM, rp3);
        v.push_back(fs);
        fs.bumpIndex();
      }
}

// Points
/////////

void Points::getFragments(const Mat4& outerM, const Camera& cam,
                          FragmentVector& v)
{
  fragparams.path = &path;
  fragparams.scaleedges = scaleedges;

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
      Vec4 pt = outerM*Vec4(x[i], y[i], z[i], 1);
      fp.points[0] = vec4to3(pt);
      fp.proj[0] = calcProjVec(cam.perspM, pt);
      if(hassizes)
        fp.pathsize = sizes[i];

      if(fp.points[0].isfinite())
        {
          v.push_back(fp);
          fp.bumpIndex();
        }
    }
}

// ObjectContainer
//////////////////

ObjectContainer::~ObjectContainer()
{
  for(unsigned i=0, s=objects.size(); i<s; ++i)
    delete objects[i];
}


void ObjectContainer::getFragments(const Mat4& outerM, const Camera& cam,
				   FragmentVector& v)
{
  Mat4 totM(outerM*objM);
  unsigned s=objects.size();
  for(unsigned i=0; i<s; ++i)
    objects[i]->getFragments(totM, cam, v);
}
