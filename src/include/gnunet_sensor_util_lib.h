/*
     This file is part of GNUnet.
     (C)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file sensor/sensor_util_lib.c
 * @brief senor utilities
 * @author Omar Tarabai
 */

#ifndef GNUNET_SENSOR_UTIL_LIB_H
#define GNUNET_SENSOR_UTIL_LIB_H

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif

/**
 * Structure containing sensor definition
 */
struct SensorInfo
{

  /**
   * The configuration handle
   * carrying sensor information
   */
  struct GNUNET_CONFIGURATION_Handle *cfg;

  /*
   * Sensor name
   */
  char *name;

  /*
   * Path to definition file
   */
  char *def_file;

  /*
   * First part of version number
   */
  uint16_t version_major;

  /*
   * Second part of version number
   */
  uint16_t version_minor;

  /*
   * Sensor description
   */
  char *description;

  /*
   * Sensor currently enabled
   */
  int enabled;

  /*
   * Category under which the sensor falls (e.g. tcp, datastore)
   */
  char *category;

  /*
   * When does the sensor become active
   */
  struct GNUNET_TIME_Absolute *start_time;

  /*
   * When does the sensor expire
   */
  struct GNUNET_TIME_Absolute *end_time;

  /*
   * Time interval to collect sensor information (e.g. every 1 min)
   */
  struct GNUNET_TIME_Relative interval;

  /*
   * Lifetime of an information sample after which it is deleted from storage
   * If not supplied, will default to the interval value
   */
  struct GNUNET_TIME_Relative lifetime;

  /*
   * A set of required peer capabilities for the sensor to collect meaningful information (e.g. ipv6)
   */
  char *capabilities;

  /*
   * Either "gnunet-statistics" or external "process"
   */
  char *source;

  /*
   * Name of the GNUnet service that is the source for the gnunet-statistics entry
   */
  char *gnunet_stat_service;

  /*
   * Name of the gnunet-statistics entry
   */
  char *gnunet_stat_name;

  /**
   * Handle to statistics get request (OR GNUNET_SCHEDULER_NO_TASK)
   */
  struct GNUNET_STATISTICS_GetHandle *gnunet_stat_get_handle;

  /*
   * Name of the external process to be executed
   */
  char *ext_process;

  /*
   * Arguments to be passed to the external process
   */
  char *ext_args;

  /*
   * Handle to the external process
   */
  struct GNUNET_OS_CommandHandle *ext_cmd;

  /*
   * Did we already receive a value
   * from the currently running external
   * proccess ? #GNUNET_YES / #GNUNET_NO
   */
  int ext_cmd_value_received;

  /*
   * The output datatype to be expected
   */
  char *expected_datatype;

  /*
   * Peer-identity of peer running collection point
   */
  struct GNUNET_PeerIdentity *collection_point;

  /*
   * Time interval to send sensor information to collection point (e.g. every 30 mins)
   */
  struct GNUNET_TIME_Relative collection_interval;

  /*
   * Flag specifying if value is to be communicated to the p2p network
   */
  int p2p_report;

  /*
   * Time interval to communicate value to the p2p network
   */
  struct GNUNET_TIME_Relative p2p_interval;

  /*
   * Execution task (OR GNUNET_SCHEDULER_NO_TASK)
   */
  GNUNET_SCHEDULER_TaskIdentifier execution_task;

  /*
   * Is the sensor being executed
   */
  int running;

};

/**
 * Carries a single reading from a sensor
 */
struct GNUNET_SENSOR_Reading
{

  /**
   * Sensor this reading is related to
   */
  struct SensorInfo *sensor;

  /**
   * Timestamp of taking the reading
   */
  uint64_t timestamp;

  /**
   * Reading value
   */
  void *value;

  /**
   * Size of @value
   */
  uint16_t value_size;

};

GNUNET_NETWORK_STRUCT_BEGIN

/**
 * Used to communicate sensor readings to
 * collection points (SENSORDASHBAORD service)
 */
struct GNUNET_SENSOR_ReadingMessage
{

  /**
   * GNUNET general message header
   */
  struct GNUNET_MessageHeader header;

  /**
   * Size of the sensor name value, allocated
   * at position 0 after this struct
   */
  uint16_t sensorname_size;

  /**
   * First part of sensor version number
   */
  uint16_t sensorversion_major;

  /**
   * Second part of sensor version number
   */
  uint16_t sensorversion_minor;

  /**
   * Timestamp of recorded reading
   */
  uint64_t timestamp;

  /**
   * Size of reading value, allocation
   * at poistion 1 after this struct
   */
  uint16_t value_size;

};
GNUNET_NETWORK_STRUCT_END

/**
 * Reads sensor definitions from local data files
 *
 * @return a multihashmap of loaded sensors
 */
struct GNUNET_CONTAINER_MultiHashMap *
GNUNET_SENSOR_load_all_sensors ();

/*
 * Get path to the directory containing the sensor definition files
 *
 * @return sensor files directory
 */
char *
GNUNET_SENSOR_get_sensor_dir ();

/**
 * Parses a sensor reading message struct
 *
 * @param msg message header received
 * @param sensors multihashmap of loaded sensors
 * @return sensor reading struct or NULL if error
 */
struct GNUNET_SENSOR_Reading *
GNUNET_SENSOR_parse_reading_message (const struct GNUNET_MessageHeader *msg,
    struct GNUNET_CONTAINER_MultiHashMap *sensors);

/**
 * Destroys a group of sensors in a hashmap and the hashmap itself
 *
 * @param sensors hashmap containing the sensors
 */
void
GNUNET_SENSOR_destroy_sensors (struct GNUNET_CONTAINER_MultiHashMap *sensors);

#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

/* ifndef GNUNET_SENSOR_UTIL_LIB_H */
#endif
/* end of gnunet_sensor_util_lib.h */
