/*
 *  Asterisk reverse Pickup
 *
 * Copyright (C) 2012 - 2022,  Stoian Ivanov <sdr@mail.bg>
 *
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#ifndef AST_MODULE
#define AST_MODULE "app_revpickup"
#endif

/*! \file
 *
 * \brief Asterisk reverse Pickup 
 *
 * \author Stoian Ivanov <sdr@mail.bg>
 * 
 * \ingroup applications
 */


#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "1.0")

#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/lock.h"
#include "asterisk/app.h"
#include "asterisk/sched.h"


//#include "ast_syslog_magick.h"


static char *syn="reverse Pickup";
static char *desc="Lookup pickup groups, hangup, originate back so you can see callerID";

#define APP "revPickup"
static char *app = APP;

#define  posthupwaitms_DEF 500
static int posthupwaitms;
#define  calltimeout_DEF 15000
static int calltimeout;

struct ast_sched_context *module_sched;
int module_sched_started;


typedef struct  {
	char *channel;
	long long grp;
} revPickup_data;
 
static int revpickup_do(const void *data) {
	revPickup_data* dt=(revPickup_data*)data;
	
	ast_log(LOG_NOTICE, APP ": Enter phase 1.5; Our channel was: %s with pickup group %lld",dt->channel,dt->grp);

	
	char *tech=dt->channel, *addr=0, *lastdash=0, *cc=dt->channel;
	
	while (*cc!=0) {
		if (!addr) {
			if (*cc=='/') {
				addr=cc+1;
				*cc=0;
			}
		} else{
			if (*cc=='-') lastdash=cc;
		}
		cc++;
	}
	if (addr==0 || *addr==0) {
		ast_log(LOG_ERROR, APP ": Failed to parse chanel name (not in TECH/ADDR-ID format?)");
		ast_free(dt->channel);
		ast_free(dt);
		return 0;
	}
	
	if (lastdash) *lastdash=0;
	ast_log(LOG_NOTICE, APP ": Call tech: %s  addr: %s",tech,addr);
	
	//find a call to pickup
	
	struct ast_channel* c; 
	char * cname=NULL,*callerName=NULL,*callerNum=NULL;
	
	struct ast_channel_iterator *iter=ast_channel_iterator_all_new();
	for (; iter && (c = ast_channel_iterator_next(iter)); ast_channel_unref(c)) {
		if (ast_channel_state(c)==AST_STATE_RINGING && (ast_channel_pickupgroup(c)&dt->grp)) {
			cname=strdupa(ast_channel_name (c));
			
			struct ast_party_connected_line * cl =ast_channel_connected (c);
			if (cl->id.name.str) callerName=strdupa(cl->id.name.str);
			if (cl->id.number.str) callerNum=strdupa(cl->id.number.str);
			
			ast_log(LOG_NOTICE, APP ": Go for channel %s , pickup group %llu presenting num:%s nam:%s",cname,ast_channel_pickupgroup(c),callerNum,callerName);
			ast_channel_unref(c);
			break;
		}
		
	}
	ast_channel_iterator_destroy (iter);

	if (!cname) {
		ast_log(LOG_NOTICE, APP ": Cant find ringing channel for group %llu",dt->grp);
		ast_free(dt->channel);
		ast_free(dt);
		return 0;
	}

	
	
	//// ORIGINATE:
	struct ast_format tmpfmt;
	struct ast_format_cap *cap_slin = ast_format_cap_alloc_nolock();
	
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR12, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR16, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR24, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR32, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR44, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR48, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR96, 0));
	ast_format_cap_add(cap_slin, ast_format_set(&tmpfmt, AST_FORMAT_SLINEAR192, 0));

	
 	ast_pbx_outgoing_app 	( 	tech, cap_slin, addr, //const char *type, struct ast_format_cap *cap, const char *addr
								calltimeout, app,cname, //int timeout, const char *app, const char *appdata
 								NULL, 0,callerNum,callerName, // int *reason, int sync, const char *cid_num, const char *cid_name
 								NULL,NULL,NULL //struct ast_variable *vars, const char *account, struct ast_channel **locked_channel
 	) ;
	
	cap_slin = ast_format_cap_destroy(cap_slin);
	
	ast_free(dt->channel);
	ast_free(dt);
	ast_log(LOG_NOTICE, APP ": Phase 1.5 done");
	return 0;
}

static int revpickup_exec(struct ast_channel *chan, const char *appdata)
{

	long long pgrp=ast_channel_pickupgroup(chan);
	if (pgrp==0) {
		ast_log(LOG_NOTICE, APP ": %s has no pickup group set so we'll just hangup",ast_channel_name (chan));
		ast_softhangup 	( chan,	AST_SOFTHANGUP_DEV);
		return 1;
	}
	if (appdata && *appdata!=0) {
		ast_log(LOG_NOTICE, APP ": Enter phase 2;  Going for %s",appdata);
		int found=0;
		struct ast_channel* target;
		struct ast_channel_iterator *iter=ast_channel_iterator_all_new();
		for (; iter && (target = ast_channel_iterator_next(iter)); ast_channel_unref(target)) {
			if (ast_channel_state(target)==AST_STATE_RINGING && (strcmp(appdata,ast_channel_name (target))==0)) {
				ast_channel_lock(target);
				found=1;
				break;
			}
			
		}
		ast_channel_iterator_destroy (iter);
		
		if (found){
			ast_log(LOG_NOTICE, APP ": %s found ringing",appdata);
			int res = ast_do_pickup(chan, target);
			ast_channel_unlock(target);
			ast_channel_unref(target);
			ast_log(LOG_NOTICE, APP ": Phase 2 done");
			return res;
		} else {
			ast_log(LOG_NOTICE, APP ": %s not found or not ringing! Phase 2 done",appdata);
			return 1;
		}
	}
	
	ast_log(LOG_NOTICE, APP ": Enter phase 1");
	
	
// 	ast_channel_lock (chan);
// 	struct varshead * vh=ast_channel_varshead(chan);
// 	if (!vh) {
// 		syslog (SYSLOG_NOTICE,APP " No VH! \n");
// 		
// 	} else{
// 		syslog (SYSLOG_NOTICE,APP " Got VH! \n");
// 		struct ast_var_t *variables;
// 		AST_LIST_TRAVERSE(vh, variables, entries) {
// 			syslog (SYSLOG_NOTICE,APP " vn: %s vv: %s \n",ast_var_name(variables), ast_var_value(variables));
// 		}
// 	}
// 	
// 	struct ast_party_caller* cl =ast_channel_caller ( chan );
// 	
// 	syslog (SYSLOG_NOTICE,APP " ani: %p id: %p(%s) priv:  %p \n",cl->ani.name.str,cl->id.name.str,cl->id.name.str,cl->priv.name.str);
// 	
// 	ast_channel_unlock (chan);
// 	
// 	return;
	
	revPickup_data* dt=ast_calloc (1,sizeof(revPickup_data)) ;
	dt->channel=ast_strdup(ast_channel_name (chan));
	dt->grp=ast_channel_pickupgroup(chan);
	
	//ast_hangup(chan);
	
	if (!module_sched_started) {
		module_sched_started=1;
		ast_log(LOG_NOTICE, APP ": Starting " AST_MODULE " schedule thread");
		ast_sched_start_thread (module_sched);
	}
	if (ast_sched_add (module_sched,posthupwaitms, revpickup_do, dt)==-1) {
		ast_log(LOG_ERROR, APP ": FAILED to create schedule event for callback! (%s)",dt->channel);
		ast_free(dt->channel);
		ast_free(dt);
	};
	ast_softhangup 	( chan,	AST_SOFTHANGUP_DEV);
	ast_log(LOG_NOTICE, APP ": Phase 1 done");
	return 1;
}

static int unload_module(void)
{
	struct ast_sched_context *tmp=module_sched;
	module_sched=NULL;
	ast_sched_context_destroy(tmp);
	return ast_unregister_application(app);
}

static int load_module(void)
{
	
	if ( sizeof(ast_group_t)!=sizeof(unsigned long long)){
		ast_log(LOG_ERROR,"This module is build agains unsupported asterisk headers :(");
		return  AST_MODULE_LOAD_DECLINE;
	}
	posthupwaitms=posthupwaitms_DEF;
	calltimeout=calltimeout_DEF;
	
	struct ast_flags flags={0};
	struct ast_config*  cfg=ast_config_load2("revpickup.conf",AST_MODULE,flags);
	if (cfg) {
		const char * s;
		
		s=ast_variable_retrieve ( cfg,"general", "posthupwaitms");
		if (s) {
			posthupwaitms=atoi(s);
			if (posthupwaitms<=0) posthupwaitms=posthupwaitms_DEF;
		} 

		s=ast_variable_retrieve ( cfg,"general", "calltimeout");
		if (s) {
			if (calltimeout<=0) calltimeout=calltimeout_DEF;
		} 
		
		
		ast_config_destroy(cfg);
	}
	
	module_sched_started=0;
	module_sched=ast_sched_context_create();
	ast_log(LOG_NOTICE,"revPickup Application is now (re)loaded posthupwaitms:%d calltimeout:%d",posthupwaitms,calltimeout);
	return ast_register_application(app, revpickup_exec,syn,desc);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "revPickup Application");
