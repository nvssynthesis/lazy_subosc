/*
 this is a suboscillator with lazy tracking, allowing it to potentially be flat in a
 manner unrelated to integer divisions of the main (input) oscillator.
 it accomplishes this by using a probability function.
 -actually, this will still lock it to change sign based on integer-related zero crossings;
 it won't change sign in the middle of a cycle.
 
 arguments:
     -count: the number of zero crossings to wait before trying to change sign
     -prob: the likelihood that it will indeed change sign
 inputs:
     -signal: expects a phasor~
     -message in same inlet: can be "count $1" or "prob $1"
 
 outputs:
     -subdivided main out
 -reported frequency
 */

#include "m_pd.h"
#include <stdlib.h>
#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

/* ------------------------ lazy_subosc~ ----------------------------- */


static t_class *lazy_subosc_class;

typedef struct _lazy_subosc
{
    t_object x_obj;
    t_float x_f;
    t_sample x_lastin;
    t_sample x_lastout;
    t_int x_signout; // -0 or 1, would be cool to use bitfield instead
    t_int x_subd;
    t_float x_prob;
    t_int x_wavlenCount, x_freq;
    t_float x_fs;
    
    t_outlet *x_sig_outlet;
    t_outlet *x_f_outlet;
} t_lazy_subosc;

static t_int *lazy_subosc_perform(t_int *w)
{
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_lazy_subosc *x = (t_lazy_subosc *)(w[3]);
    int blocksize = (int)(w[4]);
    
    int i;
    t_sample lastin = x->x_lastin;
    t_int signout = x->x_signout;
    t_int subd = x->x_subd;
    t_float prob = x->x_prob;
    t_int wavlenCount = x->x_wavlenCount;
    t_int freq = x->x_freq;
    t_float fs = x->x_fs;
    
    t_float norm = (t_float)((subd - 1) * 0.5f); // used for both offset and normalize
    t_float randf;
    
    for (i = 0; i < blocksize; i++, in1++)
    {
        t_sample phasorIn = *in1; // crazy bug: was using = *in++
        if (phasorIn < lastin){
            randf = ((t_float)(rand() % 10000)) / 10000.f;
            if (randf < prob) {
                signout += 1;
                signout %= subd;
                if (signout == 0) {
                    freq = fs / (t_float)wavlenCount;
                    wavlenCount = 0;
                }
            }
        }
        wavlenCount += 1;
        *out++ = (t_sample)((signout - norm) / norm);
        lastin = phasorIn;
    }
    outlet_float(x->x_f_outlet, freq);
    
    x->x_lastin = lastin;
    x->x_signout = signout;
    x->x_wavlenCount = wavlenCount;
    x->x_freq = freq;
    return (w+5);
}

static void lazy_subosc_dsp(t_lazy_subosc *x, t_signal **sp)
{
    x->x_fs = sp[0]->s_sr;
    
    dsp_add(lazy_subosc_perform, 4,
            sp[0]->s_vec, sp[1]->s_vec, //sp[2]->s_vec,
            x, sp[0]->s_n);
}

static void *lazy_subosc_new(void)
{
    t_lazy_subosc *x = (t_lazy_subosc *)pd_new(lazy_subosc_class);
    //inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_sig_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_f_outlet = outlet_new(&x->x_obj, &s_float);
    
    srand(23);
    
    x->x_lastin = 0;
    x->x_lastout = 0;
    x->x_signout = 1;
    x->x_prob = 1.f;
    x->x_subd = 2;
    x->x_f = 0;
    x->x_wavlenCount = 0;
    x->x_freq = 0.f;
    return (x);
}

static void lazy_subosc_free(t_lazy_subosc *x)
{
    outlet_free(x->x_sig_outlet);
    outlet_free(x->x_f_outlet);
}

static void lazy_subosc_set_subd(t_lazy_subosc *x, t_float f)
{
    t_int subd = (t_int)f;
    subd = subd >= 2 ? subd : 2;
    x->x_subd = subd;
}

static void lazy_subosc_set_prob(t_lazy_subosc *x, t_float f)
{
    f = f > 0.f ? (f < 1.f ? f : 1.f) : 0.f;
    x->x_prob = f;
}

void lazy_subosc_tilde_setup(void)
{
    lazy_subosc_class = class_new(gensym("lazy_subosc~"), (t_newmethod)lazy_subosc_new, (t_method)lazy_subosc_free,
                             sizeof(t_lazy_subosc), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(lazy_subosc_class, t_lazy_subosc, x_f);
    class_addmethod(lazy_subosc_class, (t_method)lazy_subosc_set_subd,
                    gensym("subd"), A_FLOAT, 0);
    class_addmethod(lazy_subosc_class, (t_method)lazy_subosc_set_prob,
                    gensym("prob"), A_FLOAT, 0);
    class_addmethod(lazy_subosc_class, (t_method)lazy_subosc_dsp, gensym("dsp"), 0);
}

