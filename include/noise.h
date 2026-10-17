#ifndef NOISE_H
#define NOISE_H


float lerp(float a, float b, float t); 
float fade(float t); 
float grad(int hash, float x, float z); 
float perlin2D(float x, float z); 

#endif 
