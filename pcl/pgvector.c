/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgvector.c */
/* HP-GL/2 vector commands */

#include "stdio_.h"		/* for gdebug.h */
#include "gdebug.h"
#include "pgmand.h"
#include "pggeom.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "gspath.h"
#include "gscoord.h"
#include "math_.h"

/* ------ Internal procedures ------ */

/* Draw an arc (AA, AR). */
private int
hpgl_arc(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	
	hpgl_real_t x_center, y_center, sweep, x_current, y_current, chord_angle = 5;
	hpgl_real_t radius, start_angle;

	if ( !hpgl_arg_units(pargs, &x_center) ||
	     !hpgl_arg_units(pargs, &y_center) ||
	     !hpgl_arg_c_real(pargs, &sweep)
	     )
	  return e_Range;

	hpgl_arg_c_real(pargs, &chord_angle);

	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

	x_current = pgls->g.pos.x;
	y_current = pgls->g.pos.y;

	if ( relative ) 
	  {
	    x_center += x_current;
	    y_center += y_current;
	  }

	radius = 
	  hpgl_compute_distance(x_current, y_current, x_center, y_center);

	start_angle = radians_to_degrees *
	  hpgl_compute_angle(x_current - x_center, y_current - y_center);

	hpgl_add_arc_to_path(pgls, x_center, y_center, 
			     radius, start_angle, sweep, 
			     (sweep < 0.0 ) ?
			     -chord_angle : chord_angle);

	hpgl_draw_current_path(pgls, hpgl_rm_vector);

	return 0;
}


/* Draw a 3-point arc (AT, RT). */
/* HAS check if these can be connected to other portions of the
   current subpath */

private int
hpgl_arc_3_point(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	
	hpgl_real_t x_inter, y_inter, x_end, y_end, x_start, y_start, chord_angle = 5;
	int counter_clockwise;
	hpgl_real_t x_center, y_center, radius, start_angle, end_angle;

	if ( !hpgl_arg_units(pargs, &x_inter) ||
	     !hpgl_arg_units(pargs, &y_inter) ||
	     !hpgl_arg_units(pargs, &x_end) ||
	     !hpgl_arg_units(pargs, &y_end)
	     )
		return e_Range;

	hpgl_arg_c_real(pargs, &chord_angle);

	if ( relative ) 
	  {
	    x_inter += pgls->g.pos.x; 
	    y_inter += pgls->g.pos.y;
	    x_end += pgls->g.pos.x; 
	    y_end += pgls->g.pos.y;
	  }

	x_start = pgls->g.pos.x;
	y_start = pgls->g.pos.y;
	
	if ( hpgl_3_same_points(x_start, y_start, x_inter, 
				y_inter, x_end, y_end) ) 
	  hpgl_draw_dot(pgls, x_start, y_start);

	else if ( hpgl_3_no_intermediate(x_start, y_start, x_inter, 
					 y_inter, x_end, y_end) ) 
	  hpgl_draw_line(pgls, x_start, y_start, x_end, y_end);

	else if ( hpgl_3_same_endpoints(x_start, y_start, x_inter, 
				      y_inter, x_end, y_end) ) 
	  hpgl_add_arc_to_path(pgls, (x_start + x_inter) / 2.0,
			       (y_start + y_inter) / 2.0,
			       (hypot((x_inter - x_start),
				      (y_inter - y_start)) / 2.0),
			       0.0, 360.0, chord_angle);

	else if ( hpgl_3_colinear_points(x_start, y_start, x_inter, 
					 y_inter, x_end, y_end) ) 
	  
	  if ( hpgl_3_intermediate_between(x_start, y_start, x_inter, 
					   y_inter, x_end, y_end) ) 
	    hpgl_draw_line(pgls, x_start, y_start, x_end, y_end);
	  else 
	    {
	      hpgl_draw_line(pgls, x_start, y_start, 
			       x_inter, y_inter);
	      hpgl_draw_line(pgls, x_start, y_start,
			       x_end, y_end);
	    }
	else 
	  {
	    hpgl_compute_arc_center(x_start, y_start, 
				    x_inter, y_inter, 
				    x_end, y_end, &x_center, &y_center);
	    
	    radius = hypot(x_start - x_center, y_start - y_center);
	    
	    counter_clockwise = 
	      hpgl_compute_arc_direction(x_start, y_start, x_inter, y_inter);
	    
	    start_angle = radians_to_degrees *
	      hpgl_compute_angle(x_start - x_center, y_start - y_center);

	    end_angle = radians_to_degrees *
	      hpgl_compute_angle(x_end - x_center, y_end - y_center);
	    
	    {
	      floatp sweep_angle, dot_product;
	      hpgl_args_t args;
	      gs_point u, v, u_scaled, v_scaled;

	      /* calculate angle x between start vector (u) and end vector 
		 (v) using:
		            u         v
		  cos(x) =----   .  ----
		          ||u||     ||v||      
	       */

	      u.x = x_start - x_center;
	      u.y = y_start - y_center;
	      
	      v.x = x_end - x_center;
	      v.y = y_end - y_center;
	      
	      hpgl_call(hpgl_scale_vector(&u, (1.0 / radius), &u_scaled));
	      hpgl_call(hpgl_scale_vector(&v, (1.0 / radius), &v_scaled));
	      
	      hpgl_call(hpgl_compute_dot_product(&u_scaled, 
						 &v_scaled,
						 &dot_product));
	      			
	      /* HAS annoying imprecision -- clamp for now but needs
                 to be done correctly later specifically dot_product
                 +- NOISE results in acos returning NaN */
	      dot_product = (dot_product > 1.0) ? 1.0 : dot_product;
	      dot_product = (dot_product < -1.0) ? -1.0 : dot_product;
	      sweep_angle = radians_to_degrees * acos(dot_product);



	      if ( hpgl_compute_arc_direction(x_start, y_start, 
					      x_inter, y_inter) )
		{
		  sweep_angle = -sweep_angle;
		  sweep_angle += 360.0;
		  sweep_angle = -sweep_angle;
		}
	      else
		sweep_angle = 360.0 - sweep_angle;
		  
	      /* reverse the direction if necessary.  hpgl_arc handles
                 changing the chord value. */

	      hpgl_args_set_real(&args, x_center);
	      hpgl_args_add_real(&args, y_center);
	      hpgl_args_add_real(&args, sweep_angle);
	      hpgl_args_add_real(&args, chord_angle);
	      hpgl_arc(&args, pgls, false);
	    }
	  }
	return 0;
}

/* Draw a Bezier (BR, BZ). */
int
hpgl_bezier(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	
	hpgl_real_t x_start, y_start;
	/*
	 * Since these commands take an arbitrary number of arguments,
	 * we reset the argument bookkeeping after each group.
	 */
	for ( ; ; )
	  { 
	    hpgl_real_t coords[6];
	    int i;
	    
	    for ( i = 0; i < 6 && hpgl_arg_units(pargs, &coords[i]); ++i )
	      ;
	    switch ( i )
	      {
	      case 0:		/* done */
		return e_Unimplemented;
	      case 6:
		break;
	      default:
		return e_Range;
	      }
	    
	    x_start = pgls->g.pos.x;
	    y_start = pgls->g.pos.x;
	    
	    if ( relative )
	      hpgl_add_bezier_to_path(pgls, x_start, y_start,
				      x_start + coords[0], 
				      y_start + coords[1],
				      x_start + coords[2], 
				      y_start + coords[3],
				      x_start + coords[4], 
				      y_start + coords[5]);
	    else
	      hpgl_add_bezier_to_path(pgls, x_start, y_start, 
				      coords[0], coords[1], 
				      coords[2], coords[3], 
				      coords[4], coords[5]);
	    
	    
	    /* Prepare for the next set of points. */
	    hpgl_args_init(pargs);
	  }
}

/* plots and clears lost mode if appropriate. */
private int
hpgl_plot_and_clear(hpgl_state_t *pgls, floatp x, floatp y,
		    int (*gs_func)(gs_state *pgs, floatp x, floatp y))
{
	int ret = hpgl_add_point_to_path(pgls, x, y, gs_func);
	if ( (!pgls->g.relative) &&
	     (ret != gs_error_limitcheck) )
	  hpgl_set_lost_mode(pgls, hpgl_lost_mode_cleared);
	    
	return ret;
}


/* Plot points, symbols, or lines (PA, PD, PR, PU). */
int
hpgl_plot(hpgl_args_t *pargs, hpgl_state_t *pgls, 
	  int (*gs_func)(gs_state *pgs, floatp x, floatp y))
{	

	/*
	 * Since these commands take an arbitrary number of arguments,
	 * we reset the argument bookkeeping after each group.
	 * Note: handle case of 0 args.
	 */

	hpgl_real_t x, y;
	bool got_args = false;

	while ( hpgl_arg_units(pargs, &x) && hpgl_arg_units(pargs, &y) ) 
	  { 
	    got_args = true;
	    hpgl_call(hpgl_plot_and_clear(pgls, x, y, gs_func));
	    /* Prepare for the next set of points. */
	    hpgl_args_init(pargs);
	  }

	/* no argument case */
	if ( !got_args) 
	  {
	    /* HAS needs investigation -- NO ARGS case in relative mode */
	    if (pgls->g.relative)
	      {
		hpgl_call(hpgl_plot_and_clear(pgls, 0.0, 0.0, gs_func));
	      }
	    else
	      {
		gs_point cur_point;
		hpgl_call(hpgl_get_current_position(pgls, &cur_point));
		hpgl_call(hpgl_plot_and_clear(pgls, cur_point.x, 
					  cur_point.y, gs_func));
	      }
	  }

	/* set the current state position */
	return 0;
}

/* ------ Commands ------ */

/* AA xcenter,ycenter,sweep[,chord]; */
int
hpgl_AA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_arc(pargs, pgls, false);
}

/* AR xcenter,ycenter,sweep[,chord]; */
int
hpgl_AR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_arc(pargs, pgls, true);
}

/* AT xinter,yinter,xend,yend[,chord]; */
int
hpgl_AT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_arc_3_point(pargs, pgls, false);
}

/* BR x1,y1,x2,y2,x3,y3...; */
int
hpgl_BR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_bezier(pargs, pgls, true);
}

/* BZ x1,y1,x2,y2,x3,y3...; */
int
hpgl_BZ(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_bezier(pargs, pgls, false);
}

/* CI radius[,chord]; */
int
hpgl_CI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t radius, chord = 5;
	hpgl_pen_state_t saved_pen_state;
	if ( !hpgl_arg_units(pargs, &radius) )
	  return e_Range;
	hpgl_arg_c_real(pargs, &chord);
	/* draw the current path if there is one */
	hpgl_draw_current_path(pgls, hpgl_rm_vector);
	hpgl_save_pen_state(pgls, &saved_pen_state, hpgl_pen_all);

	pgls->g.pen_down = true;

	/* draw the arc/circle */
	hpgl_add_arc_to_path(pgls, pgls->g.pos.x, pgls->g.pos.y,
			       radius, 0.0, 360.0, chord);
	/* restore pen state */

	hpgl_restore_pen_state(pgls, &saved_pen_state, hpgl_pen_all);
	hpgl_draw_current_path(pgls, hpgl_rm_vector);

	return 0;
}

/* PA x,y...; */
int
hpgl_PA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	/* set the state flag */
	pgls->g.relative = false;
	/* plot the point and save the return value */
	return hpgl_plot(pargs, pgls, 
			 (pgls->g.pen_down) ? gs_lineto : gs_moveto);
}

/* PD (d)x,(d)y...; */
int
hpgl_PD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.pen_down = true;
	return hpgl_plot(pargs, pgls, 
			 (pgls->g.relative) ? gs_rlineto : gs_lineto);
}

/* PE (flag|value|coord)*; */
/*
 * We record the state of the command in the 'phase' as follows:
 */
enum {
	pe_pen_up = 1,	/* next coordinate are pen-up move */
	pe_absolute = 2,	/* next coordinates are absolute */
	pe_7bit = 4,		/* use 7-bit encoding */
};
#define pe_fbits_shift 3	/* ... + # of fraction bits << this much */
private bool pe_args(P3(hpgl_args_t *, int32 *, int));
int
hpgl_PE(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	/*
	 * To simplify the control structure here, we require that
	 * the input buffer be large enough to hold 2 coordinates
	 * in the base 32 encoding, i.e., 14 bytes.  We're counting on
	 * there not being any whitespace within coordinate values....
	 */
	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;

	while ( p < rlimit )
	  { byte ch = *(pargs->source.ptr = ++p);
	  switch ( ch & 127 /* per spec */ )
	    {
	    case ';':
	      return 0;
	    case ':':
	      if_debug0('I', "\n  :");
	      { int32 pen;
	      if ( !pe_args(pargs, &pen, 1) )
		{ pargs->source.ptr = p - 1;
		break;
		}
	      {
		hpgl_args_t args;
		hpgl_args_set_int(&args, pen);
		/* Note SP is illegal in polygon mode we must handle that here */
		if ( !pgls->g.polygon_mode )
		  hpgl_call(hpgl_SP(&args, pgls));
	      }
	      }
	      p = pargs->source.ptr;
	      continue;
	    case '<':
	      if_debug0('I', "\n  <");
	      pargs->phase |= pe_pen_up;
	      continue;
	    case '>':
	      if_debug0('I', "\n  >");
	      { int32 fbits;
	      if ( !pe_args(pargs, &fbits, 1) )
		{ pargs->source.ptr = p - 1;
		break;
		}
	      if ( fbits < -26 || fbits > 26 )
		return e_Range;
	      pargs->phase =
		(pargs->phase & (-1 << pe_fbits_shift)) +
		(fbits << pe_fbits_shift);
	      }
	      p = pargs->source.ptr;
	      continue;
	    case '=':
	      if_debug0('I', "  =");
	      pargs->phase |= pe_absolute;
	      continue;
	    case '7':
	      if_debug0('I', "\n  7");
	      pargs->phase |= pe_7bit;
	      continue;
	    case ESC:
		/*
		 * This is something of a hack.  Apparently we're supposed
		 * to parse all PCL commands embedded in the GL/2 stream,
		 * and simply ignore everything except the 3 that end GL/2
		 * mode.  Instead, we simply stop parsing PE arguments as
		 * soon as we see an ESC.
		 */
		if ( ch == ESC ) /* (might be ESC+128) */
		  { pargs->source.ptr = p - 1; /* rescan ESC */
		    return 0;
		  }
		/* falls through */
	    default:
	      if ( (ch & 127) <= 32 || (ch & 127) == 127 )
		continue;
	      pargs->source.ptr = p - 1;
	      { 
		int32 xy[2];
		if ( !pe_args(pargs, xy, 2) )
		  break;
		{ 
		  hpgl_args_t args;
		
		  /* set up the up and down state */
		  hpgl_args_setup(&args);
		  if ( pargs->phase & pe_pen_up ) 
		    hpgl_call(hpgl_PU(&args, pgls));
		  else
		    hpgl_call(hpgl_PD(&args, pgls));
		
		  hpgl_args_set_real2(&args, (floatp)xy[0], (floatp)xy[1]);

		  /* arbitrarily use PA or PR */
		  if ( pargs->phase & pe_absolute )
		    hpgl_call(hpgl_PA(&args, pgls));
		  else
		    hpgl_call(hpgl_PR(&args, pgls));
		}
	      }
	      pargs->phase &= ~(pe_pen_up | pe_absolute);
	      p = pargs->source.ptr;
	      continue;
	    }
	  break;
	  }
	return e_NeedData;
}
/* Get an encoded value from the input.  Return false if we ran out of */
/* input data.  Ignore syntax errors (!). */
private bool
pe_args(hpgl_args_t *pargs, int32 *pvalues, int count)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;
	int i;

	for ( i = 0; i < count; ++i )
	  { int32 value = 0;
	    int shift = 0;
	    
	    for ( ; ; )
	      { int ch;
	      
	      if ( p >= rlimit )
		return false;
	      ch = *++p;
	      if ( (ch & 127) <= 32 || (ch & 127) == 127 )
		continue;
	      if ( pargs->phase & pe_7bit )
		{ ch -= 63;
		if ( ch & ~63 )
		  goto syntax_error;
		value += (int32)(ch & 31) << shift;
		shift += 5;
		if ( ch & 32 )
		  break;
		}
	      else
		{ ch -= 63;
		if ( ch & ~191 )
		  goto syntax_error;
		value += (int32)(ch & 63) << shift;
		shift += 6;
		if ( ch & 128 )
		  break;
		}
	      }
	    pvalues[i] = (value & 1 ? -(value >> 1) : value >> 1);
	    if_debug2('I', "  [%ld,%d]", (long)pvalues[i],
		      (int)arith_rshift(pargs->phase, pe_fbits_shift));
	  }
	pargs->source.ptr = p;
	return true;
syntax_error:
	/* Just ignore everything we've parsed up to this point. */
	pargs->source.ptr = p;
	return false;
}

/* PR dx,dy...; */
int
hpgl_PR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.relative = true;
	return hpgl_plot(pargs, pgls, 
			 ((pgls->g.pen_down) ? gs_rlineto : gs_rmoveto));
}

/* PU (d)x,(d)y...; */
int
hpgl_PU(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	pgls->g.pen_down = false;
	return hpgl_plot(pargs, pgls, (pgls->g.relative) ? gs_rmoveto : gs_moveto);
}

/* RT xinter,yinter,xend,yend[,chord]; */
int
hpgl_RT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_arc_3_point(pargs, pgls, true);
}

/* Initialization */
private int
pgvector_do_init(gs_memory_t *mem)
{	
	/* Register commands */
	DEFINE_HPGL_COMMANDS
		HPGL_COMMAND('A', 'A', hpgl_AA, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('A', 'R', hpgl_AR, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('A', 'T', hpgl_AT, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('B', 'R', hpgl_BR, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('B', 'Z', hpgl_BZ, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('C', 'I', hpgl_CI, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
		HPGL_COMMAND('P', 'A', hpgl_PA, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('P', 'D', hpgl_PD, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('P', 'E', hpgl_PE, hpgl_cdf_polygon),
		HPGL_COMMAND('P', 'R', hpgl_PR, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),	/* argument pattern can repeat */
		HPGL_COMMAND('P', 'U', hpgl_PU, hpgl_cdf_polygon),	/* argument pattern can repeat */
		HPGL_COMMAND('R', 'T', hpgl_RT, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgvector_init = {
	pgvector_do_init, 0
};
