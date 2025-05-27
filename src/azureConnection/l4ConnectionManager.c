#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>

#include "deviceReboot.h"
#include "l4ConnectionManager.h"

LOG_MODULE_REGISTER(l4ConnectionManager, LOG_LEVEL_INF);

#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

static K_SEM_DEFINE(network_connected_sem, 0, 1);

l4ConnectionManagerEventCb connectionManagerHandler;

static void on_net_event_l4_connected(void)
{
	k_sem_give(&network_connected_sem);
	
	l4ConnectionManagerEvent ev;
	ev.event=L4_CNCT_MNG_NETWORK_CONNECTED;
	(*connectionManagerHandler)(&ev);
	
}

static void on_net_event_l4_disconnected(void)
{
	l4ConnectionManagerEvent ev;
	ev.event=L4_CNCT_MNG_NETWORK_DISCONNECTED;
	(*connectionManagerHandler)(&ev);
}

static void l4_event_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface)
{
	switch (event) 
	{
		case NET_EVENT_L4_CONNECTED:
			LOG_INF("Network connectivity established and IP address assigned");
			on_net_event_l4_connected();
		break;
		case NET_EVENT_L4_DISCONNECTED:
			LOG_INF("Network connectivity lost, no IP address assigned");
			on_net_event_l4_disconnected();
		break;

		default:
		return;
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) 
	{
		LOG_ERR("Fatal error received from the connectivity layer, rebooting");
		DeviceRebootError();
	}
}

//__________________________________________________________________________________
//Functions available outside module scope:
//Init network manager module for connection 
int L4ConnectionManagerNetworkInit(l4ConnectionManagerEventCb l4ConnectionManagerEventHandler)
{
    LOG_INF("Network init");

	connectionManagerHandler = l4ConnectionManagerEventHandler;

	// Setup handler for Zephyr NET Connection Manager events.
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	// Setup handler for Zephyr NET Connection Manager Connectivity layer.
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	LOG_INF("Network interface is now initialized");
	return 0;

}

//Connect to network
int L4ConnectionManagerNetworkConnect(uint16_t timeoutSeconds)
{
   int err;

	// Turn off modem power saving mode (PSM) to ensure that the modem is always awake.
	//lte_lc_psm_req(false);

	err = conn_mgr_all_if_up(true);
	if (err) 
	{
		LOG_ERR("conn_mgr_if_connect, error: %d", err);
		return err;
	}

	err = conn_mgr_all_if_connect(true);
	if (err) 
	{
		LOG_ERR("conn_mgr_all_if_connect, error: %d", err);
		return err;
	}

	// If interface was already up, we need to wait for the status to be resent.
	conn_mgr_mon_resend_status();

   err = k_sem_take(&network_connected_sem, K_SECONDS(timeoutSeconds));
	if (err != 0)
	{
		LOG_ERR("Could not connect to network");
		return err;
	}

	LOG_INF("Connected to network");
	return 0;
}

//Disconnect from network
int L4ConnectionManagerNetworkDisconnect()
{
    int err;

	err = conn_mgr_all_if_down(true);
	if (err) 
	{
		LOG_ERR("conn_mgr_all_if_down, error: %d", err);
		return err;
	}

	err = conn_mgr_all_if_disconnect(true);
	if (err) 
	{
		LOG_ERR("conn_mgr_all_if_disconnect, error: %d", err);
		return err;
	}

    LOG_INF("Disconnected from network");
	return 0;
}
//__________________________________________________________________________________