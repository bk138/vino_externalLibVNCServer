<?php
/*
 * Copyright (C) 2008 Jorge Pereira <jorge@jorgepereira.com.br>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Jorge Pereira <jorge@jorgepereira.com.br>
 */

/** default timeout */
define (TIMEOUT, 10);

/** default port of vino server */
define (DEFAULT_PORT, 5900);

/** Version of service */
define (VERSION, "0.1");

/** disable message errors */
error_reporting(0);

/**
 * Return the version of service
 *
 * @return string
 */
function vino_get_version ($method_name = null, $params = null, $app_data = null)
{
   return VERSION;
}

/**
 * This function return the ip of client
 *
 * @return string
 */
function vino_get_ip ($method_name = null, $params = null, $app_data = null)
{
   return $_SERVER['REMOTE_ADDR'];
}

/**
 * This function check and return array with fields below
 *
 * @return array
 * + @field string  ip 
 * + @field boolean status
 * + @field string  version
 */
function vino_check ($method_name = null, $params = null, $app_data = null)
{
   return array (ip      => vino_get_ip (),
                 status  => vino_check_port ($method_name, $params, $app_data),
                 version => vino_get_version ()
   );
}

/**
 * This function verify if possible create a socket with client host 
 * using the port in paramenter.
 *
 * @return bool
 */
function vino_check_port ($method_name = null, $params = null, $app_data = null)
{
  $port    = $params[0] > 0 ? $params[0] : DEFAULT_PORT;
  $timeout = $params[1] > 0 ? $params[1] : TIMEOUT;
  $server  = vino_get_ip ();
  $status  = -1;

  if ($server and $port and $timeout)
  {
    $status = @fsockopen("$server", $port, $errno, $errstr, $timeout);
  }

  return ($status) ? true : false;
}

/**
 * Create a server
 */
$obj = xmlrpc_server_create ();

/**
 * Register the methods
 */
xmlrpc_server_register_method ($obj, "vino.get_ip", "vino_get_ip");
xmlrpc_server_register_method ($obj, "vino.get_version", "vino_get_version");
xmlrpc_server_register_method ($obj, "vino.check_port", "vino_check_port");
xmlrpc_server_register_method ($obj, "vino.check", "vino_check");

/**
 * Call the method
 */
echo xmlrpc_server_call_method ($obj, $GLOBALS['HTTP_RAW_POST_DATA'], '');

/**
 * Destroy the xmlrpc object
 */
xmlrpc_server_destroy ($obj);

?>
