/*
** Copyright (c) 2002-2021, Erik de Castro Lopo <erik@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

/*
** API documentation is available here:
**     http://libsamplerate.github.io/libsamplerate/
*/

#ifndef SAMPLERATE_H
#define SAMPLERATE_H

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */


/* Opaque data type SRC_STATE. */
typedef struct SRC_STATE_tag SRC_STATE ;

/* SRC_DATA is used to pass data to src_simple() and src_process(). */
typedef struct
{	const float	*data_in ;
	float	*data_out ;

	long	input_frames, output_frames ;
	long	input_frames_used, output_frames_gen ;

	int		end_of_input ;

	double	src_ratio ;
} SRC_DATA ;

/* SRC_CB_DATA is used with callback based API. */
typedef struct
{	long	frames ;
	const float	*data_in ;
} SRC_CB_DATA ;

/*
** User supplied callback function type for use with src_callback_new()
** and src_callback_read(). First parameter is the same pointer that was
** passed into src_callback_new(). Second parameter is pointer to a
** pointer. The user supplied callback function must modify *data to
** point to the start of the user supplied float array. The user supplied
** function must return the number of frames that **data points to.
*/

typedef long (*src_callback_t) (void *cb_data, float **data) ;

/*
** The following enums can be used to set the interpolator type
** using the function src_new().
*/

enum
{
	SRC_SINC_BEST_QUALITY		= 0,
	SRC_SINC_MEDIUM_QUALITY		= 1,
	SRC_SINC_FASTEST			= 2,
	SRC_ZERO_ORDER_HOLD			= 3,
	SRC_LINEAR					= 4,
} ;

/*
** The following enums can be used to set the interpolator type
** using the function src_set_ratio().
*/

enum
{
	SRC_FALSE					= 0,
	SRC_TRUE					= 1,
} ;

/*
** The following enums can be used to set the dither type
** using the function src_set_ratio().
*/

enum
{
	SRC_ERR_NO_ERROR			= 0,

	SRC_ERR_MALLOC_FAILED		= 1,
	SRC_ERR_BAD_STATE			= 2,
	SRC_ERR_BAD_DATA			= 3,
	SRC_ERR_BAD_DATA_PTR		= 4,
	SRC_ERR_NO_PRIVATE			= 5,
	SRC_ERR_BAD_SRC_RATIO		= 6,
	SRC_ERR_BAD_PROC_PTR		= 7,
	SRC_ERR_SHIFT_BITS			= 8,
	SRC_ERR_FILTER_LEN			= 9,
	SRC_ERR_BAD_CONVERTER		= 10,
	SRC_ERR_BAD_CHANNEL_COUNT	= 11,
	SRC_ERR_SINC_BAD_BUFFER_LEN	= 12,
	SRC_ERR_SIZE_INCOMPATIBILITY= 13,
	SRC_ERR_BAD_PRIV_PTR		= 14,
	SRC_ERR_BAD_SINC_STATE		= 15,
	SRC_ERR_DATA_OVERLAP		= 16,
	SRC_ERR_BAD_CALLBACK		= 17,
	SRC_ERR_BAD_MODE			= 18,
	SRC_ERR_NULL_CALLBACK		= 19,
	SRC_ERR_NO_VARIABLE_RATIO	= 20,
	SRC_ERR_SINC_PREPARE_DATA_BAD_LEN	= 21,
	SRC_ERR_BAD_INTERNAL_STATE	= 22,

	/* This must be the last error number. */
	SRC_ERR_MAX_ERROR			= 23

} ;

/* Standard initialisation function : return an anonymous pointer to the
** internal state of the converter. Choose a converter from the enums below.
** Error returned in *error.
*/

SRC_STATE* src_new (int converter_type, int channels, int *error) ;

/* Initilisation for callback based API : return an anonymous pointer to the
** internal state of the converter. Choose a converter from the enums below.
** The cb_data pointer can point to any data or be set to NULL. Whatever the
** value, when processing, user supplied function "func" gets called with
** cb_data as first parameter.
*/

SRC_STATE* src_callback_new (src_callback_t func, int converter_type, int channels,
								int *error, void* cb_data) ;

/* Cleanup all internal allocations.
** Always returns NULL.
*/

SRC_STATE* src_delete (SRC_STATE *state) ;

/* Standard processing function.
** Returns non zero on error.
*/

int src_process (SRC_STATE *state, SRC_DATA *data) ;

/* Callback based processing function. Read up to frames worth of data from
** the converter int *data and return frames read or -1 on error.
*/
long src_callback_read (SRC_STATE *state, double src_ratio, long frames, float *data) ;

/* Simple interface for performing a single rate conversion with a single
** call. It should be used when processing a single buffer of samples.
** Simple interface does not require initialisation as it can only operate on
** a single buffer worth of audio.
*/
int src_simple (SRC_DATA *data, int converter_type, int channels) ;

/*
** This library contains a number of different sample rate converters,
** numbered 0 through N.
**
** Return a string giving either a name or a more full description of each
** sample rate converter or NULL if no sample rate converter exists for
** the given value. The converters are sequentially numbered from 0 to N.
*/

const char *src_get_name (int converter_type) ;
const char *src_get_description (int converter_type) ;
const char *src_get_version (void) ;

/*
** Set a new SRC ratio. This allows step responses
** in the conversion ratio.
** Returns non zero on error.
*/

int src_set_ratio (SRC_STATE *state, double new_ratio) ;

/*
** Get the current channel count.
** Returns negative on error, positive channel count otherwise
*/

int src_get_channels (SRC_STATE *state) ;

/*
** Reset the internal SRC state.
** Does not modify the quality settings.
** Does not free any memory allocations.
** Returns non zero on error.
*/

int src_reset (SRC_STATE *state) ;

/*
** Return TRUE if ratio is a valid conversion ratio, FALSE
** otherwise.
*/

int src_is_valid_ratio (double ratio) ;

/*
** Return an error number.
*/

int src_error (SRC_STATE *state) ;

/*
** Convert the error number into a string.
*/
const char* src_strerror (int error) ;

/*
** The following enums can be used to set the interpolator type
** using the function src_set_ratio().
*/

enum
{
	SRC_ERR_SHIFT_BITS			= 8,
	SRC_ERR_FILTER_LEN			= 9,
	SRC_ERR_BAD_CONVERTER		= 10,
	SRC_ERR_BAD_CHANNEL_COUNT	= 11
} ;

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

#endif	/* SAMPLERATE_H */