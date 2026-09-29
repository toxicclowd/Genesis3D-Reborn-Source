#include <cmath>
#include <cstdlib>

#include "noise.h"
#include "mtl9937.h"

Noise::HashTable::HashTable (int32 seed)
{
    int32 i, j, temp;
    srand (seed);
    for (i=0; i<256; ++i) 
        value[i] = i;
    for (i=255; i>=0; --i) {
        j = rand() % 256;
        temp = value[i];
        value[i] = value[j];
        value[j] = temp;
    }
}

const Noise::HashTable Noise::hTable (14);

Noise::VecTable::VecTable (int32 seed)
{
    grFloat l;
    srand (seed);
    for (int32 m=0; m<256; ++m) {
        do {
            x[m] = Dblrand (-1, 1);
            y[m] = Dblrand (-1, 1);
            z[m] = Dblrand (-1, 1);
            o[m] = Dblrand (-0.5, 0.5);
        } while ((l = x[m]*x[m] + y[m]*y[m] + z[m]*z[m]) > 1.0);
    l = 1.0f/(grFloat)sqrt (l);
    x[m] *= l; y[m] *= l; z[m] *= l;
    }
}

const Noise::VecTable  Noise::vTable(14);

const grVec3d Noise::bigvect = {1000.2f, 1000.5f, 1000.7f};

grFloat Noise::NoiseAt (const grVec3d &vec) const
{
    grFloat x=vec.X+1e6f, y=vec.Y+1e6f, z=vec.Z+1e6f;
    int ix=int(x), iy=int(y), iz=int(z);

    grFloat fx0=x-ix, fx1=fx0-1, fy0=y-iy, fy1=fy0-1, fz0=z-iz, fz1=fz0-1;
    grFloat wx = Scurve(fx0), wy = Scurve(fy0), wz = Scurve(fz0);

    grFloat vx0, vx1, vy0, vy1, vz0, vz1;
    int i;

    i = hTable (ix, iy, iz);
    vx0 = vTable.x[i]*fx0 + vTable.y[i]*fy0 + vTable.z[i]*fz0 + vTable.o[i];
    i = hTable (ix+1, iy, iz);
    vx1 = vTable.x[i]*fx1 + vTable.y[i]*fy0 + vTable.z[i]*fz0 + vTable.o[i];
    vy0 = vx0 + wx*(vx1-vx0);

    i = hTable (ix, iy+1, iz);
    vx0 = vTable.x[i]*fx0 + vTable.y[i]*fy1 + vTable.z[i]*fz0 + vTable.o[i];
    i = hTable (ix+1, iy+1, iz);
    vx1 = vTable.x[i]*fx1 + vTable.y[i]*fy1 + vTable.z[i]*fz0 + vTable.o[i];
    vy1 = vx0 + wx*(vx1-vx0);
    vz0 = vy0 + wy*(vy1-vy0);

    i = hTable (ix, iy, iz+1);
    vx0 = vTable.x[i]*fx0 + vTable.y[i]*fy0 + vTable.z[i]*fz1 + vTable.o[i];
    i = hTable (ix+1, iy, iz+1);
    vx1 = vTable.x[i]*fx1 + vTable.y[i]*fy0 + vTable.z[i]*fz1 + vTable.o[i];
    vy0 = vx0 + wx*(vx1-vx0);

    i = hTable (ix, iy+1, iz+1);
    vx0 = vTable.x[i]*fx0 + vTable.y[i]*fy1 + vTable.z[i]*fz1 + vTable.o[i];
    i = hTable (ix+1, iy+1, iz+1);
    vx1 = vTable.x[i]*fx1 + vTable.y[i]*fy1 + vTable.z[i]*fz1 + vTable.o[i];
    vy1 = vx0 + wx*(vx1-vx0);
    vz1 = vy0 + wy*(vy1-vy0);

    return (vz0 + wz*(vz1-vz0));
}

grVec3d Noise::DNoiseAt (const grVec3d &vec) const
{
    grVec3d diff;
    grVec3d_Subtract(&vec,&bigvect,&diff);
    grVec3d sum;
    grVec3d_Add(&vec,&bigvect,&sum);
    grVec3d result;
    grVec3d_Set(&result,NoiseAt (diff),NoiseAt (vec),NoiseAt (sum));
    return result;
}

grFloat Noise::fBm (grVec3d loc, grFloat lambda, grFloat omega, uint32 octaves) const
{
    grFloat factor=1;
    grFloat val = NoiseAt (loc); 
    for (uint32 i=0; i<(octaves-1); ++i) {
        grVec3d_Scale(&loc,lambda,&loc);
        factor *= omega;
        val += NoiseAt(loc)*factor;
    }
    return val;
}

grVec3d Noise::DfBm (grVec3d loc, grFloat lambda, grFloat omega, uint32 octaves) const
{
    grFloat factor=1;
    grVec3d val = DNoiseAt (loc); 
    for (uint32 i=0; i<(octaves-1); ++i) {
        grVec3d_Scale(&loc,lambda,&loc);
        factor *= omega;
        grVec3d tmp = DNoiseAt(loc);
        grVec3d_Scale(&tmp,factor,&tmp);
        grVec3d_Add(&tmp,&val,&val);
    }
    return val;
}

grFloat Noise::Turbulence (grVec3d loc, grFloat lambda, grFloat omega, uint32 octaves) const
{
    grFloat factor=1;
    grFloat val = (grFloat)fabs(NoiseAt (loc)); 
    for (uint32 i=0; i<(octaves-1); ++i) {
        grVec3d_Scale(&loc,lambda,&loc);
        factor *= omega;
        val += factor*(grFloat)fabs(NoiseAt (loc));
    }
    return val;
}

Noise2::HashTable::HashTable (int32 seed)
{
    int32 i, j, temp;
    srand (seed);
    for (i=0; i<256; ++i) 
        value[i] = i;
    for (i=255; i>=0; i--) {
        j = rand() % 256;
        temp = value[i];
        value[i] = value[j];
        value[j] = temp;
    }
}

const Noise2::HashTable Noise2::hTable (14);

Noise2::VecTable::VecTable (int32 seed)
{
    grFloat l;
    srand (seed);
    for (int32 m=0; m<256; ++m) {
        do {
            x[m] = Dblrand (-1, 1);
            y[m] = Dblrand (-1, 1);
            o[m] = Dblrand (-0.5, 0.5);
        } while ((l = x[m]*x[m] + y[m]*y[m]) > 1.0);
    l = 1.0f / (grFloat)sqrt (l);
    x[m] *= l; y[m] *= l;
    }
}

const Noise2::VecTable  Noise2::vTable (14);

grFloat Noise2::NoiseAt (grFloat x, grFloat y) const
{
    x+=1e6;
    y+=1e6;
    int ix=int(x), iy=int(y);

    grFloat fx0=x-ix, fx1=fx0-1, fy0=y-iy, fy1=fy0-1;
    grFloat wx = Scurve(fx0), wy = Scurve(fy0);

    grFloat vx0, vx1, vy0, vy1;
    int i;

    i = hTable (ix, iy);
    vx0 = vTable.x[i]*fx0 + vTable.y[i]*fy0 + vTable.o[i];
    i = hTable (ix+1, iy);
    vx1 = vTable.x[i]*fx1 + vTable.y[i]*fy0 + vTable.o[i];
    vy0 = vx0 + wx*(vx1-vx0);

    i = hTable (ix, iy+1);
    vx0 = vTable.x[i]*fx0 + vTable.y[i]*fy1 + vTable.o[i];
    i = hTable (ix+1, iy+1);
    vx1 = vTable.x[i]*fx1 + vTable.y[i]*fy1 + vTable.o[i];
    vy1 = vx0 + wx*(vx1-vx0);

    return (vy0 + wy*(vy1-vy0));
}

grFloat Noise2::fBm (grFloat x, grFloat y, grFloat lambda, grFloat omega, uint32 octaves) const
{
    grFloat val = NoiseAt (x, y); 
    grFloat factor = 1;
    for (uint32 i=0; i<(octaves-1); ++i) {
        x *= lambda;
        y *= lambda;
        factor *= omega;
        val += factor*NoiseAt (x, y);
    }
    return val;
}

grFloat Noise2::Turbulence (grFloat x, grFloat y, grFloat lambda, grFloat omega, uint32 octaves) const
{
    grFloat factor = 1;
    grFloat val = (grFloat)fabs(NoiseAt (x, y)); 
    for (uint32 i=0; i<(octaves-1); ++i) {
        x *= lambda;
        y *= lambda;
        factor *= omega;
        val += factor*(grFloat)fabs(NoiseAt (x, y));
    }
    return val;
}
