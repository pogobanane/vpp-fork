/*
 * Copyright (c) 2015 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * @file
 * @brief Sample Plugin, plugin API / trace / CLI handling.
 */

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <benchplug/sample.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>

/* define message IDs */
#include <benchplug/sample_msg_enum.h>

/* define message structures */
#define vl_typedefs
#include <benchplug/sample_all_api_h.h> 
#undef vl_typedefs

/* define generated endian-swappers */
#define vl_endianfun
#include <benchplug/sample_all_api_h.h> 
#undef vl_endianfun

/* instantiate all the print functions we know about */
#define vl_print(handle, ...) vlib_cli_output (handle, __VA_ARGS__)
#define vl_printfun
#include <benchplug/sample_all_api_h.h> 
#undef vl_printfun

/* Get the API version number */
#define vl_api_version(n,v) static u32 api_version=(v);
#include <benchplug/sample_all_api_h.h>
#undef vl_api_version

#define REPLY_MSG_ID_BASE sm->msg_id_base
#include <vlibapi/api_helper_macros.h>

/* List of message types that this plugin understands */

#define foreach_sample_plugin_api_msg                           \
_(SAMPLE_MACSWAP_ENABLE_DISABLE, sample_macswap_enable_disable)

/* *INDENT-OFF* */
VLIB_PLUGIN_REGISTER () = {
    .version = SAMPLE_PLUGIN_BUILD_VER,
    .description = "Sample of VPP Plugin",
};
/* *INDENT-ON* */

sample_main_t sample_main;

/**
 * @brief Enable/disable the macswap plugin. 
 *
 * Action function shared between message handler and debug CLI.
 */

int sample_macswap_enable_disable (sample_main_t * sm, u32 sw_if_index,
                                   int enable_disable)
{
  vnet_sw_interface_t * sw;
  int rv = 0;

  /* Utterly wrong? */
  if (pool_is_free_index (sm->vnet_main->interface_main.sw_interfaces, 
                          sw_if_index))
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  /* Not a physical port? */
  sw = vnet_get_sw_interface (sm->vnet_main, sw_if_index);
  if (sw->type != VNET_SW_INTERFACE_TYPE_HARDWARE)
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;
  
  vnet_feature_enable_disable ("device-input", "sample",
                               sw_if_index, enable_disable, 0, 0);

  return rv;
}

static clib_error_t *
macswap_enable_disable_command_fn (vlib_main_t * vm,
                                   unformat_input_t * input,
                                   vlib_cli_command_t * cmd)
{
  sample_main_t * sm = &sample_main;
  u32 sw_if_index = ~0;
  int enable_disable = 1;
    
  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT) {
    if (unformat (input, "disable"))
      enable_disable = 0;
    else if (unformat (input, "%U", unformat_vnet_sw_interface,
                       sm->vnet_main, &sw_if_index))
      ;
    else
      break;
  }

  if (sw_if_index == ~0)
    return clib_error_return (0, "Please specify an interface...");
    
  rv = sample_macswap_enable_disable (sm, sw_if_index, enable_disable);

  switch(rv) {
  case 0:
    break;

  case VNET_API_ERROR_INVALID_SW_IF_INDEX:
    return clib_error_return 
      (0, "Invalid interface, only works on physical ports");
    break;

  case VNET_API_ERROR_UNIMPLEMENTED:
    return clib_error_return (0, "Device driver doesn't support redirection");
    break;

  default:
    return clib_error_return (0, "sample_macswap_enable_disable returned %d",
                              rv);
  }
  return 0;
}

/**
 * @brief CLI command to enable/disable the sample macswap plugin.
 */
VLIB_CLI_COMMAND (sr_content_command, static) = {
    .path = "sample macswap",
    .short_help = 
    "sample macswap <interface-name> [disable]",
    .function = macswap_enable_disable_command_fn,
};


#include <vnet/l2/l2_fib.h>

/* correlates to the moongen incAndWrap method (does basically the same) */
static void
incr_mac_address (u8 * mac)
{
  u64 tmp = *((u64 *) mac);
  tmp = clib_net_to_host_u64 (tmp);
  tmp += 1 << 17;   /* skip unused (least significant) octets and multicast bit */
  tmp = clib_host_to_net_u64 (tmp);

  clib_memcpy (mac, &tmp, 6);
}

static clib_error_t *
benchplug_macadd_command_fn (vlib_main_t * vm,
           unformat_input_t * input, vlib_cli_command_t * cmd)
{
  vnet_main_t *nvm = vnet_get_main();
  clib_error_t *error = 0;
  u8 mac[6], save_mac[6];
  u32 bd_index = 1;
  u32 sw_if_index = ~0;
  u32 is_add = 0;
  u32 if_set = 0;
  u32 count = 1;
  int mac_set = 0;
  int i;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "mac %U", unformat_ethernet_address, mac))
  mac_set = 1;
      else if (unformat (input, "add"))
  is_add = 1;
      else if (unformat (input, "int %U", unformat_vnet_sw_interface, nvm, &sw_if_index))
  if_set = 1;
      else if (unformat (input, "count %d", &count))
  ;
      else
  break;
    }

  if (mac_set == 0)
    return clib_error_return (0, "mac not set");
  if (if_set == 0)
    return clib_error_return (0, "interface not set");

  if (is_add == 0)
    return clib_error_return (0,
            "noop: pick at least one of (add)");

  clib_memcpy (save_mac, mac, 6);

  if (is_add)
    {
      for (i = 0; i < count; i++)
  {
    l2fib_add_entry (mac, bd_index, sw_if_index,
         L2FIB_ENTRY_RESULT_FLAG_NONE);
    incr_mac_address (mac);
  }
    }

  return error;
}

/**
 * @brief CLI command to enable/disable the sample macswap plugin.
 * benchplugaddbd1 add count 10 mac 20:00:00:00:00:00 int local0
 */
VLIB_CLI_COMMAND (benchplug_addbd1, static) = {
    .path = "benchplugaddbd1",
    .short_help = 
    "benchplugaddbd1 add count <count> mac <macDstBase> int <intDst>",
    .function = benchplug_macadd_command_fn,
};

static clib_error_t *
ai_ctl_command_fn (vlib_main_t * vm,
           unformat_input_t * input, vlib_cli_command_t * cmd)
{
  sample_main_t * sm = &sample_main;
  u32 is_start = 0;
  u32 is_stop = 0;
  u32 is_comm = 0;
  u32 is_last_answer = 0;

  clib_error_t *error = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "start"))
  is_start = 1;
      else if (unformat (input, "stop"))
  is_stop = 1;
      else if (unformat (input, "comm"))
  is_comm = 1;
      else if (unformat (input, "last_answer"))
  is_last_answer = 1;
      else
  break;
    }

  if (is_start)
    {
	    // TODO initialize ipc
	    int ret = sample_ipc_open(&(sm->ipc));
	    if (ret < 0) {
		return clib_error_return(0, "sample ipc error %d", ret);
	    }

    }
  else if (is_comm)
    {
      vlib_cli_output (vm, "AI: communicating...");
      sample_ipc_communicate_to_server(&(sm->ipc), 1, 2, 666);
      vlib_cli_output (vm, "done. Answer poll1: %d", sm->ipc.last_response.poll1);

    }
  else if (is_stop)
    {
	error = sample_ipc_close(&(sm->ipc));
    }
  else if (is_last_answer)
    {
      vlib_cli_output(vm, "Last received answer:");
      vlib_cli_output(vm, "    udelay:        %d", sm->ipc.last_response.udelay);
      vlib_cli_output(vm, "    usleep:        %d", sm->ipc.last_response.usleep);
      vlib_cli_output(vm, "    use_interrupt: %d", sm->ipc.last_response.use_interrupt);
    }

  return error;
}


/**
 * @brief Proof of concept implementation for vpp performance 
 * optimization using AI (idp-okelmann, vpp-benchplug)
 */
VLIB_CLI_COMMAND (ai_ctl, static) = {
    .path = "ai",
    .short_help = 
    "ai [ start | comm | stop | last_answer ]",
    .function = ai_ctl_command_fn,
};

/**
 * @brief Plugin API message handler.
 */
static void vl_api_sample_macswap_enable_disable_t_handler
(vl_api_sample_macswap_enable_disable_t * mp)
{
  vl_api_sample_macswap_enable_disable_reply_t * rmp;
  sample_main_t * sm = &sample_main;
  int rv;

  rv = sample_macswap_enable_disable (sm, ntohl(mp->sw_if_index), 
                                      (int) (mp->enable_disable));
  
  REPLY_MACRO(VL_API_SAMPLE_MACSWAP_ENABLE_DISABLE_REPLY);
}

/**
 * @brief Set up the API message handling tables.
 */
static clib_error_t *
sample_plugin_api_hookup (vlib_main_t *vm)
{
  sample_main_t * sm = &sample_main;
#define _(N,n)                                                  \
    vl_msg_api_set_handlers((VL_API_##N + sm->msg_id_base),     \
                           #n,					\
                           vl_api_##n##_t_handler,              \
                           vl_noop_handler,                     \
                           vl_api_##n##_t_endian,               \
                           vl_api_##n##_t_print,                \
                           sizeof(vl_api_##n##_t), 1); 
    foreach_sample_plugin_api_msg;
#undef _

    return 0;
}

#define vl_msg_name_crc_list
#include <benchplug/sample_all_api_h.h>
#undef vl_msg_name_crc_list

static void 
setup_message_id_table (sample_main_t * sm, api_main_t *am)
{
#define _(id,n,crc) \
  vl_msg_api_add_msg_name_crc (am, #n "_" #crc, id + sm->msg_id_base);
  foreach_vl_msg_name_crc_sample;
#undef _
}

/**
 * @brief Initialize the sample plugin.
 */
static clib_error_t * sample_init (vlib_main_t * vm)
{
  sample_main_t * sm = &sample_main;
  clib_error_t * error = 0;
  u8 * name;

  sm->vnet_main =  vnet_get_main ();

  name = format (0, "sample_%08x%c", api_version, 0);

  /* Ask for a correctly-sized block of API message decode slots */
  sm->msg_id_base = vl_msg_api_get_msg_ids 
      ((char *) name, VL_MSG_FIRST_AVAILABLE);

  error = sample_plugin_api_hookup (vm);

  /* Add our API messages to the global name_crc hash table */
  setup_message_id_table (sm, &api_main);

  vec_free(name);

  return error;
}

VLIB_INIT_FUNCTION (sample_init);

/**
 * @brief Hook the sample plugin into the VPP graph hierarchy.
 */
VNET_FEATURE_INIT (sample, static) = 
{
  .arc_name = "device-input",
  .node_name = "sample",
  .runs_before = VNET_FEATURES ("ethernet-input"),
};
