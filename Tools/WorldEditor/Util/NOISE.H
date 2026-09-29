#ifndef NOISE_H
#define NOISE_H

#include <vec3d.h>

inline float Dblrand (float min, float max)
{
  return (max-min) * (float(rand())/RAND_MAX) + min;
}

inline float Scurve (float val)
{
    return (val*val*(3.0f - 2.0f*val));  
}

class Noise
{
private:
        class HashTable
        {
        private:
                int32               value[256];
        public:
                                    HashTable (int32 seed);

                int                 operator() (int32 i1, int32 i2, int32 i3) const
                                    {
                                        return value[value[value[i1%256]^(i2%256)]^(i3%256)];
                                    }
        };

        class VecTable
        {
        public:
                grFloat             x[256], y[256], z[256], o[256];

                                    VecTable (int32 seed);
        };

static  const HashTable             hTable;
static  const VecTable              vTable;
static  const grVec3d                bigvect;

public:
        grFloat                     NoiseAt (const grVec3d &) const;
        grVec3d                     DNoiseAt (const grVec3d &) const;

        grFloat                     fBm (grVec3d, grFloat, grFloat, uint32) const;
        grVec3d                     DfBm (grVec3d, grFloat, grFloat, uint32) const;

        grFloat                     Turbulence (grVec3d, grFloat, grFloat, uint32) const;
};

class Noise2
{
private:
        class HashTable
        {
        private:
                uint32              value[256];
        public:
                                    HashTable (int32 seed);

                int                 operator() (int32 i1, int32 i2) const
                                    { return value[value[i1%256]^(i2%256)]; }
        };

        class VecTable
        {
        public:
                grFloat             x[256], y[256], o[256];

                                    VecTable (int32 seed);
        };

static  const HashTable             hTable;
static  const VecTable              vTable;

public:
        grFloat                     NoiseAt (grFloat, grFloat) const;

        grFloat                     fBm (grFloat, grFloat, grFloat, grFloat, uint32) const;

        grFloat                     Turbulence (grFloat, grFloat, grFloat, grFloat, uint32) const;
};

#endif
