/*
    chips.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998-2003 Frodo Looijaard <frodol@dds.nl>
                            and Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (c) 2003-2006 The lm_sensors team

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "chips.h"
#include "lib/sensors.h"
#include "lib/error.h"

void print_chip_raw(const sensors_chip_name *name)
{
	int a, b;
	const sensors_feature *feature;
	const sensors_subfeature *sub;
	char *label;
	double val;

	a = 0;
	while ((feature = sensors_get_features(name, &a))) {
		if (!(label = sensors_get_label(name, feature))) {
			printf("ERROR: Can't get feature label!\n");
			continue;
		}
		printf("%s:\n", label);
		free(label);

		b = 0;
		while ((sub = sensors_get_all_subfeatures(name, feature, &b))) {
			if (sub->flags & SENSORS_MODE_R) {
				if (sensors_get_value(name, sub->number, &val))
					printf("ERROR: Can't get feature `%s' "
					       "data!\n", sub->name);
				else
					printf("  %s: %.2f\n", sub->name, val);
			} else
				printf("(%s)\n", label);
		}
	}
}

static inline double deg_ctof(double cel)
{
	return cel * (9.0F / 5.0F) + 32.0F;
}

static void print_label(const char *label, int space)
{
	int len = strlen(label)+1;
	printf("%s:%*s", label, space - len, "");
}

static void sensors_get_available_features(const sensors_chip_name *name,
					   const sensors_feature *feature,
					   short *has_features,
					   double *feature_vals, int size,
					   int first_val)
{
	const sensors_subfeature *iter;
	int i = 0;

	while ((iter = sensors_get_all_subfeatures(name, feature, &i))) {
		int indx, err;

		indx = iter->type - first_val;
		if (indx < 0 || indx >= size)
			/* New feature in libsensors? Ignore. */
			continue;

		err = sensors_get_value(name, iter->number, &feature_vals[indx]);
		if (err) {
			printf("ERROR: Can't get %s data: %s\n", iter->name,
			       sensors_strerror(err));
			continue;
		}

		has_features[indx] = 1;
	}
}

static int sensors_get_label_size(const sensors_chip_name *name)
{
	int i;
	const sensors_feature *iter;
	char *label;
	unsigned int max_size = 11;	/* 11 as minumum label width */

	i = 0;
	while ((iter = sensors_get_features(name, &i))) {
		if ((label = sensors_get_label(name, iter)) &&
		    strlen(label) > max_size)
			max_size = strlen(label);
		free(label);
	}
	return max_size + 1;
}

static void print_temp_limits(double limit1, double limit2,
			      const char *name1, const char *name2, int alarm)
{
	if (fahrenheit) {
		limit1 = deg_ctof(limit1);
		limit2 = deg_ctof(limit2);
        }

	if (name2) {
		printf("(%-4s = %+5.1f%s, %-4s = %+5.1f%s)  ",
		       name1, limit1, degstr,
		       name2, limit2, degstr);
	} else if (name1) {
		printf("(%-4s = %+5.1f%s)                  ",
		       name1, limit1, degstr);
	} else {
		printf("                                  ");
	}

	if (alarm)
		printf("ALARM  ");
}

#define TEMP_FEATURE(x)		has_features[x - SENSORS_SUBFEATURE_TEMP_INPUT]
#define TEMP_FEATURE_VAL(x)	feature_vals[x - SENSORS_SUBFEATURE_TEMP_INPUT]
static void print_chip_temp(const sensors_chip_name *name,
			    const sensors_feature *feature,
			    int label_size)
{
	double val, limit1, limit2;
	const char *s1, *s2;
	int alarm, crit_displayed = 0;
	char *label;
	const int size = SENSORS_SUBFEATURE_TEMP_TYPE - SENSORS_SUBFEATURE_TEMP_INPUT + 1;
	short has_features[SENSORS_SUBFEATURE_TEMP_TYPE - SENSORS_SUBFEATURE_TEMP_INPUT + 1] = { 0, };
	double feature_vals[SENSORS_SUBFEATURE_TEMP_TYPE - SENSORS_SUBFEATURE_TEMP_INPUT + 1] = { 0.0, };

	if (!(label = sensors_get_label(name, feature))) {
		printf("ERROR: Can't get temperature label!\n");
		return;
	}

	sensors_get_available_features(name, feature, has_features,
				       feature_vals, size,
				       SENSORS_SUBFEATURE_TEMP_INPUT);
	val = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_INPUT);

	alarm = TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_ALARM) &&
		TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_ALARM);
	if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MAX)) {
		if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MAX_ALARM) &&
		    TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_MAX_ALARM))
			alarm |= 1;

     		if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MIN)) {
			limit1 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_MIN);
			s1 = "low";
			limit2 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_MAX);
			s2 = "high";

			if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MIN_ALARM) &&
			    TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_MIN_ALARM))
				alarm |= 1;
		} else {
			limit1 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_MAX);
			s1 = "high";

			if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_MAX_HYST)) {
				limit2 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_MAX_HYST);
				s2 = "hyst";
			} else if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT)) {
				limit2 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT);
				s2 = "crit";

				if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT_ALARM) &&
				    TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT_ALARM))
					alarm |= 1;
				crit_displayed = 1;
			} else {
				limit2 = 0;
				s2 = NULL;
			}
		}
	} else if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT)) {
		limit1 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT);
		s1 = "crit";

		if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT_HYST)) {
			limit2 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT_HYST);
			s2 = "hyst";
		} else {
			limit2 = 0;
			s2 = NULL;
		}

		if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT_ALARM) &&
		    TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT_ALARM))
			alarm |= 1;
		crit_displayed = 1;
	} else {
		limit1 = limit2 = 0;
		s1 = s2 = NULL;
	}

	print_label(label, label_size);
	free(label);

	if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_FAULT) &&
	    TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_FAULT)) {
		printf("   FAULT  ");
	} else {
		if (fahrenheit)
			val = deg_ctof(val);
		printf("%+6.1f%s  ", val, degstr);
	}
	print_temp_limits(limit1, limit2, s1, s2, alarm);

	if (!crit_displayed && TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT)) {
		limit1 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT);
		s1 = "crit";

		if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT_HYST)) {
			limit2 = TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT_HYST);
			s2 = "hyst";
		} else {
			limit2 = 0;
			s2 = NULL;
		}

		alarm = TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_CRIT_ALARM) &&
			TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_CRIT_ALARM);

		printf("\n%*s", label_size + 10, "");
		print_temp_limits(limit1, limit2, s1, s2, alarm);
	}

	/* print out temperature sensor info */
	if (TEMP_FEATURE(SENSORS_SUBFEATURE_TEMP_TYPE)) {
		int sens = (int)TEMP_FEATURE_VAL(SENSORS_SUBFEATURE_TEMP_TYPE);

		/* older kernels / drivers sometimes report a beta value for
		   thermistors */
		if (sens > 1000)
			sens = 4;

		printf("sensor = %s", sens == 0 ? "disabled" :
		       sens == 1 ? "diode" :
		       sens == 2 ? "transistor" :
		       sens == 3 ? "thermal diode" :
		       sens == 4 ? "thermistor" :
		       sens == 5 ? "AMD AMDSI" :
		       sens == 6 ? "Intel PECI" : "unknown");
	}
	printf("\n");
}

#define IN_FEATURE(x)		has_features[x - SENSORS_SUBFEATURE_IN_INPUT]
#define IN_FEATURE_VAL(x)	feature_vals[x - SENSORS_SUBFEATURE_IN_INPUT]
static void print_chip_in(const sensors_chip_name *name,
			  const sensors_feature *feature,
			  int label_size)
{
	const int size = SENSORS_SUBFEATURE_IN_MAX_ALARM - SENSORS_SUBFEATURE_IN_INPUT + 1;
	short has_features[SENSORS_SUBFEATURE_IN_MAX_ALARM - SENSORS_SUBFEATURE_IN_INPUT + 1] = { 0, };
	double feature_vals[SENSORS_SUBFEATURE_IN_MAX_ALARM - SENSORS_SUBFEATURE_IN_INPUT + 1] = { 0.0, };
	double val, alarm_max, alarm_min;
	char *label;

	if (!(label = sensors_get_label(name, feature))) {
		printf("ERROR: Can't get in label!\n");
		return;
	}

	sensors_get_available_features(name, feature, has_features,
				       feature_vals, size,
				       SENSORS_SUBFEATURE_IN_INPUT);
	val = IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_INPUT);

	print_label(label, label_size);
	free(label);
	printf("%+6.2f V", val);

	if (IN_FEATURE(SENSORS_SUBFEATURE_IN_MIN) &&
	    IN_FEATURE(SENSORS_SUBFEATURE_IN_MAX))
		printf("  (min = %+6.2f V, max = %+6.2f V)",
		       IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_MIN),
		       IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_MAX));
	else if (IN_FEATURE(SENSORS_SUBFEATURE_IN_MIN))
		printf("  (min = %+6.2f V)",
		       IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_MIN));
	else if (IN_FEATURE(SENSORS_SUBFEATURE_IN_MAX))
		printf("  (max = %+6.2f V)",
		       IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_MAX));

	if (IN_FEATURE(SENSORS_SUBFEATURE_IN_MAX_ALARM) ||
	    IN_FEATURE(SENSORS_SUBFEATURE_IN_MIN_ALARM)) {
		alarm_max = IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_MAX_ALARM);
		alarm_min = IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_MIN_ALARM);

		if (alarm_min || alarm_max) {
			printf(" ALARM (");

			if (alarm_min)
				printf("MIN");
			if (alarm_max)
				printf("%sMAX", (alarm_min) ? ", " : "");

			printf(")");
		}
	} else if (IN_FEATURE(SENSORS_SUBFEATURE_IN_ALARM)) {
		printf("   %s",
		IN_FEATURE_VAL(SENSORS_SUBFEATURE_IN_ALARM) ? "ALARM" : "");
	}

	printf("\n");
}

#define FAN_FEATURE(x)		has_features[x - SENSORS_SUBFEATURE_FAN_INPUT]
#define FAN_FEATURE_VAL(x)	feature_vals[x - SENSORS_SUBFEATURE_FAN_INPUT]
static void print_chip_fan(const sensors_chip_name *name,
			   const sensors_feature *feature,
			   int label_size)
{
	char *label;
	const int size = SENSORS_SUBFEATURE_FAN_DIV - SENSORS_SUBFEATURE_FAN_INPUT + 1;
	short has_features[SENSORS_SUBFEATURE_FAN_DIV - SENSORS_SUBFEATURE_FAN_INPUT + 1] = { 0, };
	double feature_vals[SENSORS_SUBFEATURE_FAN_DIV - SENSORS_SUBFEATURE_FAN_INPUT + 1] = { 0.0, };
	double val;

	if (!(label = sensors_get_label(name, feature))) {
		printf("ERROR: Can't get fan label!\n");
		return;
	}

	print_label(label, label_size);
	free(label);

	sensors_get_available_features(name, feature, has_features,
				       feature_vals, size,
				       SENSORS_SUBFEATURE_FAN_INPUT);
	val = FAN_FEATURE_VAL(SENSORS_SUBFEATURE_FAN_INPUT);

	if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_FAULT) &&
	    FAN_FEATURE_VAL(SENSORS_SUBFEATURE_FAN_FAULT))
		printf("   FAULT");
	else
		printf("%4.0f RPM", val);

	if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_MIN) &&
	    FAN_FEATURE(SENSORS_SUBFEATURE_FAN_DIV))
		printf("  (min = %4.0f RPM, div = %1.0f)",
		       FAN_FEATURE_VAL(SENSORS_SUBFEATURE_FAN_MIN),
		       FAN_FEATURE_VAL(SENSORS_SUBFEATURE_FAN_DIV));
	else if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_MIN))
		printf("  (min = %4.0f RPM)",
		       FAN_FEATURE_VAL(SENSORS_SUBFEATURE_FAN_MIN));
	else if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_DIV))
		printf("  (div = %1.0f)",
		       FAN_FEATURE_VAL(SENSORS_SUBFEATURE_FAN_DIV));

	if (FAN_FEATURE(SENSORS_SUBFEATURE_FAN_ALARM) &&
	    FAN_FEATURE_VAL(SENSORS_SUBFEATURE_FAN_ALARM)) {
		printf("  ALARM");
	}

	printf("\n");
}

static void print_chip_vid(const sensors_chip_name *name,
			   const sensors_feature *feature,
			   int label_size)
{
	char *label;
	const sensors_subfeature *subfeature;
	double vid;
	int i = 0;

	subfeature = sensors_get_all_subfeatures(name, feature, &i);
	if (!subfeature)
		return;

	if ((label = sensors_get_label(name, feature))
	 && !sensors_get_value(name, subfeature->number, &vid)) {
		print_label(label, label_size);
		printf("%+6.3f V\n", vid);
	}
	free(label);
}

static void print_chip_beep_enable(const sensors_chip_name *name,
				   const sensors_feature *feature,
				   int label_size)
{
	char *label;
	const sensors_subfeature *subfeature;
	double beep_enable;
	int i = 0;

	subfeature = sensors_get_all_subfeatures(name, feature, &i);
	if (!subfeature)
		return;

	if ((label = sensors_get_label(name, feature))
	 && !sensors_get_value(name, subfeature->number, &beep_enable)) {
		print_label(label, label_size);
		printf("%s\n", beep_enable ? "enabled" : "disabled");
	}
	free(label);
}

void print_chip(const sensors_chip_name *name)
{
	const sensors_feature *feature;
	int i, label_size;

	label_size = sensors_get_label_size(name);

	i = 0;
	while ((feature = sensors_get_features(name, &i))) {
		switch (feature->type) {
		case SENSORS_SUBFEATURE_TEMP_INPUT:
			print_chip_temp(name, feature, label_size);
			break;
		case SENSORS_SUBFEATURE_IN_INPUT:
			print_chip_in(name, feature, label_size);
			break;
		case SENSORS_SUBFEATURE_FAN_INPUT:
			print_chip_fan(name, feature, label_size);
			break;
		case SENSORS_SUBFEATURE_VID:
			print_chip_vid(name, feature, label_size);
			break;
		case SENSORS_SUBFEATURE_BEEP_ENABLE:
			print_chip_beep_enable(name, feature, label_size);
			break;
		default:
			continue;
		}
	}
}
