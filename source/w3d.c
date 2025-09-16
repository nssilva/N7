/*
 * w3d.c
 * -----
 * This is a no-thoughts-what-so-ever translation of the n7 code. It's not pretty, but it works.
 * The thing is, that the n7 code was "reverse engineered" from n6 vm instructions ...
 *
 * By Marcus 2023
 */

#include "w3d.h"
#include "windowing.h"
#include "stdio.h"
#include "math.h"

/*
 * DrawSprite
 * ----------
 */
void DrawSprite(int viewX, int viewY, int viewW, int viewH,
        double fmin, double fs, unsigned char fogR, unsigned char fogG, unsigned char fogB, HashTable *zb,
        int t, int c, int x, int y, int w, int h, double d);
/*
 * W3D_Render
 * ----------
 */
Variable W3D_Render(int argc, Variable *argv) {
    Variable result;
    /* Parameters. */
    HashTable *w3d = argv[0].value.t;
    double viewX = argv[1].value.n;
    double viewZ = argv[2].value.n;
    double viewA = argv[3].value.n;
    /* Data from the w3d object. */
    HashTable *wm = ((Variable *)HT_Get(w3d, "m", 0))->value.t;
    HashTable *fm = ((Variable *)HT_Get(w3d, "fm", 0))->value.t;
    HashTable *cm = ((Variable *)HT_Get(w3d, "cm", 0))->value.t;
    HashTable *ao = ((Variable *)HT_Get(w3d, "ao", 0))->value.t;
    HashTable *aoc = ((Variable *)HT_Get(w3d, "aoc", 0))->value.t;
    HashTable *ch = ((Variable *)HT_Get(w3d, "ch", 0))->value.t;
    HashTable *zb = ((Variable *)HT_Get(w3d, "zb", 0))->value.t;
    HashTable *dl = ((Variable *)HT_Get(w3d, "d", 0))->value.t;
    HashTable *sprites = ((Variable *)HT_Get(w3d, "sprites", 0))->value.t;
    int mw = (int)((Variable *)HT_Get(w3d, "mw", 0))->value.n;
    int mh = (int)((Variable *)HT_Get(w3d, "mh", 0))->value.n;
    int vh = (int)((Variable *)HT_Get(w3d, "vh", 0))->value.n;;
    int hvh = (int)((Variable *)HT_Get(w3d, "hvh", 0))->value.n;
    int vy = (int)((Variable *)HT_Get(w3d, "vy", 0))->value.n;
    int cy = vy + hvh;
    int vw = (int)((Variable *)HT_Get(w3d, "vw", 0))->value.n;
    int vx = (int)((Variable *)HT_Get(w3d, "vx", 0))->value.n;
    double fmin = ((Variable *)HT_Get(w3d, "fmin", 0))->value.n;
    double fmax = ((Variable *)HT_Get(w3d, "fmax", 0))->value.n;
    double fmaxSqr = fmax*fmax;
    double fs = ((Variable *)HT_Get(w3d, "fs", 0))->value.n;
    double unit = ((Variable *)HT_Get(w3d, "u", 0))->value.n;
    unsigned char fogR = (unsigned char)(((Variable *)HT_Get(w3d, "fr", 0))->value.n);
    unsigned char fogG = (unsigned char)(((Variable *)HT_Get(w3d, "fg", 0))->value.n);
    unsigned char fogB = (unsigned char)(((Variable *)HT_Get(w3d, "fb", 0))->value.n);
    Variable *siVar = (Variable *)HT_Get(w3d, "si", 0);
    int si;

    /* Clear screen with fog color. */
    WIN_SetColor(fogR, fogG, fogB, 255);
    WIN_Cls(0);
    
    /* Render sky image? */
    if (siVar->type == VAR_NUM && WIN_ImageExists((int)(si = siVar->value.n))) {
		WIN_SetColor(255, 255, 255, 0);
		for (int x = 0; x < vw; x++) {
            int tileH = (int)((Variable *)HT_Get(ch, 0, x))->value.n;
            double a = viewA + ((Variable *)HT_Get(ao, 0, x))->value.n;
			while (a < 0.0) a = a + 2.0*M_PI;
			while (a > 2.0*M_PI) a = a - 2.0*M_PI;
			double u = a/(2.0*M_PI);
			WIN_DrawVRaster(si, x + vx, vy + hvh - tileH, vy + hvh, u, 0.0, u, 1.0);
		}
    }

    /* Floor and ceiling. */
    for (int y = 0; y < hvh - 4; y++) {
        double d = ((Variable *)HT_Get(dl, 0, y))->value.n;
        if (d >= fmax) break;
        int alpha = (d - fmin)*fs;
        if (alpha < 0) alpha = 0;
        else if (alpha > 255) alpha = 255;
        WIN_SetColor(fogR, fogG, fogB, (unsigned char)alpha);
        double ao0 = ((Variable *)HT_Get(ao, 0, 0))->value.n;
        double aow = ((Variable *)HT_Get(ao, 0, vw - 1))->value.n;
        double xs = viewX + d*cos(viewA + ao0);
        double zs = viewZ + d*sin(viewA + ao0);
        double xe = viewX + d*cos(viewA + aow);
        double ze = viewZ + d*sin(viewA + aow);
        double dx = xe - xs;
        double dz = ze - zs;
        d = sqrt(dx*dx + dz*dz);
        dx = dx/d;
        dz = dz/d;
        double ss = vx; // int?
        double se;
        double sdx = (double)(vw)/d; // hm -1 rly?
        int xi = floor(xs);
        int zi = floor(zs);
        double us = fmod(xs, 1);
        double vs = fmod(zs, 1);
        double ue, ve;
        do {
            int oxi = xi;
            int ozi = zi;
            double xf, zf;
            double xk, zk;
            
            /* x ray */
            if (dx < 0) {
                if (oxi < 0) break;
                xf = (double)xi;
                xi = xi - 1;
                xk = (xf - xs)/dx;
            }
            else if (dx > 0) {
                if (oxi >= mw ) break;
                xi = xi + 1;
                xf = (double)xi;
                xk = (xf - xs)/dx;
            }
            else {
                xk = 100000;
            }
            
            /* z ray */
            if (dz < 0) {
                if (ozi < 0 ) break;
                zf = (double)zi;
                zi = zi - 1;
                zk = (zf - zs)/dz;
            }
            else if (dz > 0) {
                if (ozi >= mh) break;
                zi = zi + 1;
                zf = (double)zi;
                zk = (zf - zs)/dz;
            }
            else {
                zk = 100000;
            }
            
            if (xk < zk) {
                if (dz < 0)  zi = zi + 1;
                else if (dz > 0)  zi = zi - 1;
                if (dx <= 0)  ue = 0;
                else ue = 1;
                xe = xf;
                ze = xk*dz + zs;
                ve = fmod(ze, 1);
                se = xk*sdx + ss;
            }
            else {
                if (dx < 0) xi = xi + 1;
                else if (dx > 0) xi = xi - 1;
                if (dz <= 0) ve = 0;
                else ve = 1;
                ze = zf;
                xe = zk*dx + xs;
                ue = fmod(xe, 1);
                se = zk*sdx + ss;
            }
            
            if (oxi >= 0 && oxi < mw && ozi >= 0 && ozi < mh) {
                /* Don't draw if there's a wall. */
                Variable *t = (Variable *)HT_Get(((Variable *)HT_Get(wm, 0, oxi))->value.t, 0, ozi);
                if (!(t->type == VAR_TBL && ((Variable *)HT_Get(t->value.t, "f", 0))->value.n == 0 && ((Variable *)HT_Get(t->value.t, "t", 0))->value.n != 0)) {
                    Variable *ft = (Variable *)HT_Get(((Variable *)HT_Get(fm, 0, oxi))->value.t, 0, ozi);
                    Variable *ct = (Variable *)HT_Get(((Variable *)HT_Get(cm, 0, oxi))->value.t, 0, ozi);
                    if (ft->type == VAR_NUM) WIN_DrawHRaster((int)ft->value.n, vy + vh - 1 - y, (int)ss, (int)se, us, vs, ue, ve);
                    if (ct->type == VAR_NUM) WIN_DrawHRaster((int)ct->value.n, vy + y, (int)ss, (int)se, us, vs, ue, ve);    
                }
            }
            xs = xe;
            zs = ze;
            ss = se;
            if (ue == 0) us = 1;
            else if (ue == 1) us = 0;
            else us = ue;
            if (ve == 0) vs = 1;
            else if (ve == 1) vs = 0;
            else vs = ve;
        } while ((int)se < vx + vw);
    }
    
    /* Walls */
    for (int col = 0; col < vw; col++) {
        double distx = 100000, distz = 100000;
        Variable *tx = 0, *tz = 0;
        double ux = 0, uz = 0;
        double a = viewA + ((Variable *)HT_Get(ao, 0, col))->value.n;
        double cosa = cos(a), sina = sin(a);

        /* Cast x ray. */
        if (cosa < 0) {
            double dzperdx = sina/cosa;
            for (int ix = (int)viewX - 1; ix >= 0; ix--) {
                double dx = (double)ix + 1 - viewX;
                double dz = dx*dzperdx;
                double z = viewZ + dz;
                int iz = (int)z;
                if (ix >= 0 && ix < mw && iz >= 0 && iz < mh) {
                    Variable *t = (Variable *)HT_Get(((Variable *)HT_Get(wm, 0, ix))->value.t, 0, iz);
                    if (t->type == VAR_TBL) {
                        Variable *tt = (Variable *)HT_Get(t->value.t, "e", 0);
                        if (tt->type == VAR_NUM) {
                            int tf = ((Variable *)HT_Get(t->value.t, "f", 0))->value.n;
                            if (tf == 0) {
                                distx = dx*dx + dz*dz;
                                ux = 1 - fmod(z, 1);
                                tx = tt;
                                break;
                            }
                            else if (tf == 1) {
                                double tp = ((Variable *)HT_Get(t->value.t, "p", 0))->value.n;
                                if (tp < 1) {
                                    dx = (double)ix + 0.5 - viewX;
                                    dz = dx*dzperdx;
                                    z = viewZ + dz;
                                    if ((int)z == iz && fmodf(z, 1) > tp) {
                                        distx = dx*dx + dz*dz;
                                        ux = 1 - fmod(z, 1) + tp;
                                        tx = tt;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (cosa > 0) {
            double dzperdx = sina/cosa;
            for (int ix = (int)viewX + 1; ix < mw; ix++) {
                double dx = (double)ix - viewX;
                double dz = dx*dzperdx;
                double z = viewZ + dz;
                int iz = (int)z;
                if (ix >= 0 && ix < mw && iz >= 0 && iz < mh) {
                    Variable *t = (Variable *)HT_Get(((Variable *)HT_Get(wm, 0, ix))->value.t, 0, iz);
                    if (t->type == VAR_TBL) {
                        Variable *tt = (Variable *)HT_Get(t->value.t, "w", 0);
                        if (tt->type == VAR_NUM) {
                            int tf = ((Variable *)HT_Get(t->value.t, "f", 0))->value.n;
                            if (tf == 0) {
                                distx = dx*dx + dz*dz;
                                ux = fmod(z, 1);
                                tx = tt;
                                break;
                            }
                            else if (tf == 1) {
                                double tp = ((Variable *)HT_Get(t->value.t, "p", 0))->value.n;
                                if (tp < 1) {
                                    dx = (double)ix + 0.5 - viewX;
                                    dz = dx*dzperdx;
                                    z = viewZ + dz;
                                    if ((int)z == iz && fmod(z, 1) > tp) {
                                        distx = dx*dx + dz*dz;
                                        ux = fmod(z, 1) - tp;
                                        tx = tt;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Cast z ray. */
        if (sina < 0) {
            double dxperdz = cosa/sina;
            for (int iz = (int)viewZ - 1; iz >= 0; iz--) {
                double dz = (double)iz + 1 - viewZ;
                double dx = dz*dxperdz;
                double x = viewX + dx;
                int ix = (int)x;
                if (ix >= 0 && ix < mw && iz >= 0 && iz < mh) {
                    Variable *t = (Variable *)HT_Get(((Variable *)HT_Get(wm, 0, ix))->value.t, 0, iz);
                    if (t->type == VAR_TBL) {
                        Variable *tt = (Variable *)HT_Get(t->value.t, "s", 0);
                        if (tt->type == VAR_NUM) {
                            int tf = ((Variable *)HT_Get(t->value.t, "f", 0))->value.n;
                            if (tf == 0) {
                                distz = dx*dx + dz*dz;
                                uz = fmod(x, 1);
                                tz = tt;
                                break;
                            }
                            else if (tf == 2) {
                                double tp = ((Variable *)HT_Get(t->value.t, "p", 0))->value.n;
                                if (tp < 1) {
                                    dz = (double)iz + 0.5 - viewZ;
                                    dx = dz*dxperdz;
                                    x = viewX + dx;
                                    if ((int)x == ix && fmod(x, 1) > tp) {
                                        distz = dx*dx + dz*dz;
                                        uz = fmod(x, 1) - tp;
                                        tz = tt;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (sina > 0) {
            double dxperdz = cosa/sina;
            for (int iz = (int)viewZ + 1; iz < mh; iz++) {
                double dz = (double)iz - viewZ;
                double dx = dz*dxperdz;
                double x = viewX + dx;
                int ix = (int)x;
                if (ix >= 0 && ix < mw && iz >= 0 && iz < mh) {
                    Variable *t = (Variable *)HT_Get(((Variable *)HT_Get(wm, 0, ix))->value.t, 0, iz);
                    if (t->type == VAR_TBL) {
                        Variable *tt = (Variable *)HT_Get(t->value.t, "n", 0);
                        if (tt->type == VAR_NUM) {     
                            int tf = ((Variable *)HT_Get(t->value.t, "f", 0))->value.n;                        
                            if (tf == 0) {
                                distz = dx*dx + dz*dz;
                                uz = 1 - fmod(x, 1);
                                tz = tt;
                                break;
                            }
                            else if (tf == 2) {
                                double tp = ((Variable *)HT_Get(t->value.t, "p", 0))->value.n;
                                if (tp < 1) {
                                    dz = (double)iz + 0.5 - viewZ;
                                    dx = dz*dxperdz;
                                    x = viewX + dx;
                                    if ((int)x == ix && fmod(x, 1) > tp) {
                                        distz = dx*dx + dz*dz;
                                        uz = 1 - fmod(x, 1) + tp;
                                        tz = tt;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Variable *t = 0;
        double d = 100000;
        double u = 0;
        if (tx && tz) {
            if (distx < distz) {
                t = tx;
                d = distx;
                u = ux;
            }
            else {
                t = tz;
                d = distz;
                u = uz;
            }
        }
        else if (tx) {
            t = tx;
            d = distx;
            u = ux;
        }
        else if (tz) {
            t = tz;
            d = distz;
            u = uz;
        }
        
        if (t) {
            d = sqrt(d)*((Variable *)HT_Get(aoc, 0, col))->value.n;
            if (d < fmax) {
                int ys = ceil(0.5*unit/d);
                int alpha = (d - fmin)*fs;
                if (alpha < 0) alpha = 0;
                else if (alpha > 255) alpha = 255;
                WIN_SetColor(fogR, fogG, fogB, (unsigned char)alpha);
                WIN_DrawVRaster((int)t->value.n, vx + col, cy - ys, cy + ys, u, 0, u, 1);
            }
        }
        ((Variable *)HT_Get(zb, 0, col))->value.n = d;
    }
    
    /* Sprites. */
    double ux = cos(viewA);
    double uz = sin(viewA);
    int count = HT_EntryCount(sprites);
    for (int i = 0; i < count; i++) {
        HashTable *s = ((Variable *)HT_Get(sprites, 0, i))->value.t;
        double d = ((Variable *)HT_Get(s, "d_", 0))->value.n;
        if (d < fmaxSqr) {
            double svx = ((Variable *)HT_Get(s, "x", 0))->value.n - viewX;
            double svz = ((Variable *)HT_Get(s, "z", 0))->value.n - viewZ;
            double hyp = sqrt(d);
            if (hyp > 0) {
                double k = 1/hyp;
                svx = svx*k;
                svz = svz*k;
                double dotp = svx*ux + svz*uz;
                if (dotp >= 0.1) {
                    double a, tx;
                    if (dotp > 1)  a = 0;
                    else a = acos(dotp);
                    if (ux*svz - uz*svx < 0) tx = (double)vw*0.5 - unit*tan(a);
                    else tx = (double)vw*0.5 + unit*tan(a);
                    d = hyp*cos(a);
                    if (d > 0.1 && d < fmax) {
                        double size = unit/d;
                        double spritew = ((Variable *)HT_Get(s, "w", 0))->value.n;
                        double spriteh = ((Variable *)HT_Get(s, "h", 0))->value.n;
                        int x = (int)round(tx - size*spritew*0.5);
                        int y = (int)round((double)hvh + size*(((Variable *)HT_Get(s, "y", 0))->value.n - ((Variable *)HT_Get(s, "h", 0))->value.n*0.5));
                        int w = (int)round(size*spritew);
                        int h = (int)round(size*spriteh);
                        if (x + w > 0 && x < vw) {
                            DrawSprite(
                                vx, vy, vw, vh, fmin, fs,
                                fogR, fogG, fogB, zb,
                                (int)((Variable *)HT_Get(s, "t", 0))->value.n,
                                (int)((Variable *)HT_Get(s, "c", 0))->value.n,
                                x, y, w, h, d);
                        }
                    }
                }
            }
        }
    }
    
    result.type = VAR_UNSET;

    return result;
}

/*
 * DrawSprite
 * ----------
 */
void DrawSprite(int viewX, int viewY, int viewW, int viewH,
        double fmin, double fs, unsigned char fogR, unsigned char fogG, unsigned char fogB, HashTable *zb,
        int t, int c, int x, int y, int w, int h, double d) {
    int xstart = x;
    int xend = x + w;
    int ystart = y;
    int yend = y + h;
    double du = 1/(double)(xend - xstart);
    //double dv = 1/(double)(yend - ystart);
    double u = 0;
    double v = 0;
    double vend = 1;
                    
    if (xstart >= viewW || xend < 0) return;
    if (xstart < 0) {
        u = u + du*-xstart;
        xstart = 0;
    }
    if (xend >= viewW) xend = viewW - 1;

    int alpha = (d - fmin)*fs;
    if (alpha < 0) alpha = 0;
    else if (alpha > 255) alpha = 255;
    WIN_SetColor(fogR, fogG, fogB, (unsigned char)alpha);
    for (x = xstart; x <= xend; x++) {
        if (d < ((Variable *)HT_Get(zb, 0, x))->value.n) {
            WIN_DrawVRaster(t, viewX + x, viewY + ystart, viewY + yend, u, v, u, vend);
        }
        u = u + du;
    }
}
