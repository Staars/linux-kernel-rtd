/******************************************************************************
 *
 * Copyright(c) 2007 - 2013 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

#include <drv_types.h>
#include <hal_data.h>
#include "rtw_proc.h"

#ifdef CONFIG_PROC_DEBUG

static struct proc_dir_entry *rtw_proc = NULL;

inline struct proc_dir_entry *get_rtw_drv_proc(void)
{
	return rtw_proc;
}

#define RTW_PROC_NAME DRV_NAME

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
#define file_inode(file) ((file)->f_dentry->d_inode)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#define PDE_DATA(inode) PDE((inode))->data
#define proc_get_parent_data(inode) PDE((inode))->parent->data
#endif

#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
#define get_proc_net proc_net
#else
#define get_proc_net init_net.proc_net
#endif

inline struct proc_dir_entry *rtw_proc_create_dir(const char *name, struct proc_dir_entry *parent, void *data)
{
	struct proc_dir_entry *entry;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	entry = proc_mkdir_data(name, S_IRUGO|S_IXUGO, parent, data);
#else
	//entry = proc_mkdir_mode(name, S_IRUGO|S_IXUGO, parent);
	entry = proc_mkdir(name, parent);
	if (entry)
		entry->data = data;
#endif

	return entry;
}

inline struct proc_dir_entry *rtw_proc_create_entry(const char *name, struct proc_dir_entry *parent, 
	const struct file_operations *fops, void * data)
{
	struct proc_dir_entry *entry;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	entry = proc_create_data(name,  S_IFREG|S_IRUGO|S_IWUGO, parent, fops, data);
#else
	entry = create_proc_entry(name, S_IFREG|S_IRUGO|S_IWUGO, parent);
	if (entry) {
		entry->data = data;
		entry->proc_fops = fops;
	}
#endif

	return entry;
}

static int proc_get_dummy(struct seq_file *m, void *v)
{
	return 0;
}

static int proc_get_drv_version(struct seq_file *m, void *v)
{
	dump_drv_version(m);
	return 0;
}

static int proc_get_log_level(struct seq_file *m, void *v)
{
	dump_log_level(m);
	return 0;
}

static int proc_get_drv_cfg(struct seq_file *m, void *v)
{
	dump_drv_cfg(m);
	return 0;
}

static ssize_t proc_set_log_level(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];
	int log_level;

	if (count < 1)
		return -EINVAL;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%d ", &log_level);

		if( log_level >= _drv_always_ && log_level <= _drv_debug_ )
		{
			GlobalDebugLevel= log_level;
			printk("%d\n", GlobalDebugLevel);
		}
	} else {
		return -EFAULT;
	}
	
	return count;
}

#ifdef DBG_MEM_ALLOC
static int proc_get_mstat(struct seq_file *m, void *v)
{	
	rtw_mstat_dump(m);
	return 0;
}
#endif /* DBG_MEM_ALLOC */

static int proc_get_ch_plan_test(struct seq_file *m, void *v)
{
	dump_ch_plan_test(m);
	return 0;
}

/*
* rtw_drv_proc:
* init/deinit when register/unregister driver
*/
const struct rtw_proc_hdl drv_proc_hdls [] = {
	{"ver_info", proc_get_drv_version, NULL},
	{"log_level", proc_get_log_level, proc_set_log_level},
	{"drv_cfg", proc_get_drv_cfg, NULL},
#ifdef DBG_MEM_ALLOC
	{"mstat", proc_get_mstat, NULL},
#endif /* DBG_MEM_ALLOC */
	{"ch_plan_test", proc_get_ch_plan_test, NULL},
};

const int drv_proc_hdls_num = sizeof(drv_proc_hdls) / sizeof(struct rtw_proc_hdl);

static int rtw_drv_proc_open(struct inode *inode, struct file *file)
{
	//struct net_device *dev = proc_get_parent_data(inode);
	ssize_t index = (ssize_t)PDE_DATA(inode);
	const struct rtw_proc_hdl *hdl = drv_proc_hdls+index;
       return single_open(file, hdl->show, NULL);
}

static ssize_t rtw_drv_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t index = (ssize_t)PDE_DATA(file_inode(file));
	const struct rtw_proc_hdl *hdl = drv_proc_hdls+index;
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = hdl->write;
	
	if (write)
		return write(file, buffer, count, pos, NULL);

	return -EROFS;
}

static const struct file_operations rtw_drv_proc_fops = {
	.owner = THIS_MODULE,
	.open = rtw_drv_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rtw_drv_proc_write,
};

int rtw_drv_proc_init(void)
{
	int ret = _FAIL;
	ssize_t i;
	struct proc_dir_entry *entry = NULL;

	if (rtw_proc != NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	rtw_proc = rtw_proc_create_dir(RTW_PROC_NAME, get_proc_net, NULL);

	if (rtw_proc == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	for (i=0;i<drv_proc_hdls_num;i++) {
		entry = rtw_proc_create_entry(drv_proc_hdls[i].name, rtw_proc, &rtw_drv_proc_fops, (void *)i);
		if (!entry) {
			rtw_warn_on(1);
			goto exit;
		}
	}

	ret = _SUCCESS;

exit:
	return ret;
}

void rtw_drv_proc_deinit(void)
{
	int i;

	if (rtw_proc == NULL)
		return;

	for (i=0;i<drv_proc_hdls_num;i++)
		remove_proc_entry(drv_proc_hdls[i].name, rtw_proc);

	remove_proc_entry(RTW_PROC_NAME, get_proc_net);
	rtw_proc = NULL;
}

#ifdef CONFIG_SDIO_HCI
static int proc_get_sd_f0_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	sd_f0_reg_dump(m, adapter);

	return 0;
}

static int proc_get_sdio_local_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	sdio_local_reg_dump(m, adapter);

	return 0;
}
#endif /* CONFIG_SDIO_HCI */

static int proc_get_mac_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	mac_reg_dump(m, adapter);

	return 0;
}

static int proc_get_bb_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	bb_reg_dump(m, adapter);

	return 0;
}

static int proc_get_rf_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rf_reg_dump(m, adapter);

	return 0;
}

static int proc_get_dump_adapters_status(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	dump_adapters_status(m, adapter_to_dvobj(adapter));

	return 0;
}

//gpio setting
#ifdef CONFIG_GPIO_API
static ssize_t proc_set_config_gpio(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32]={0}; 
	int num=0,gpio_pin=0,gpio_mode=0;//gpio_mode:0 input  1:output;
	
	if (count < 2)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		num	=sscanf(tmp, "%d %d",&gpio_pin,&gpio_mode);
		DBG_871X("num=%d gpio_pin=%d mode=%d\n",num,gpio_pin,gpio_mode);
      		padapter->pre_gpio_pin=gpio_pin;

		if(gpio_mode==0 || gpio_mode==1  )
			rtw_hal_config_gpio(padapter, gpio_pin,gpio_mode);	
	}
	return count;

}
static ssize_t proc_set_gpio_output_value(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32]={0}; 
	int num=0,gpio_pin=0,pin_mode=0;//pin_mode: 1 high         0:low
	
	if (count < 2)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		num	=sscanf(tmp, "%d %d",&gpio_pin,&pin_mode);
		DBG_871X("num=%d gpio_pin=%d pin_high=%d\n",num,gpio_pin,pin_mode);
		padapter->pre_gpio_pin=gpio_pin;
		
		if(pin_mode==0 || pin_mode==1  )
			rtw_hal_set_gpio_output_value(padapter, gpio_pin,pin_mode);	
	}
	return count;
}
static int proc_get_gpio(struct seq_file *m, void *v)
{
	u8 gpioreturnvalue=0;
	struct net_device *dev = m->private;
	
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	if(!padapter)	
		return -EFAULT;
	gpioreturnvalue = rtw_hal_get_gpio(padapter, padapter->pre_gpio_pin);
	DBG_871X_SEL_NL(m, "get_gpio %d:%d \n",padapter->pre_gpio_pin ,gpioreturnvalue);
	
	return 0;

}
static ssize_t proc_set_gpio(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32]={0}; 
	int num=0,gpio_pin=0;
	
	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		num	=sscanf(tmp, "%d",&gpio_pin);
		DBG_871X("num=%d gpio_pin=%d\n",num,gpio_pin);
		padapter->pre_gpio_pin=gpio_pin;
		
	}
		return count;
}
#endif


static int proc_get_linked_info_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	if(padapter)	
		DBG_871X_SEL_NL(m, "linked_info_dump :%s \n", (padapter->bLinkInfoDump)?"enable":"disable");
	
	return 0;
}

static ssize_t proc_set_linked_info_dump(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	char tmp[32]={0}; 
	int mode=0,pre_mode=0;
	int num=0;	

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	pre_mode=padapter->bLinkInfoDump;
	DBG_871X("pre_mode=%d\n", pre_mode);

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		num	=sscanf(tmp, "%d ", &mode);
		DBG_871X("num=%d mode=%d\n",num,mode);

		if(num!=1)
		{
			DBG_871X("argument number is wrong\n");
				return -EFAULT;
		}
	
		if(mode==1 || (mode==0 && pre_mode==1) ) //not consider pwr_saving 0:
		{
			padapter->bLinkInfoDump = mode;	
		
		}
		else if( (mode==2 ) || (mode==0 && pre_mode==2))//consider power_saving
		{		
			//DBG_871X("linked_info_dump =%s \n", (padapter->bLinkInfoDump)?"enable":"disable")
			linked_info_dump(padapter,mode);	
		}
	}
	return count;
}

static int proc_get_mac_qinfo(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_hal_get_hwreg(adapter, HW_VAR_DUMP_MAC_QUEUE_INFO, (u8 *)m);

	return 0;
}

int proc_get_wifi_spec(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;
	
	DBG_871X_SEL_NL(m,"wifi_spec=%d\n",pregpriv->wifi_spec);
	return 0;
}

static int proc_get_chan_plan(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *mlme = &adapter->mlmepriv;
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;

	dump_chset(m, mlmeext->channel_set);
	DBG_871X_SEL_NL(m, "Channel plan=0x%02x\n", mlme->ChannelPlan);
	return 0;
}

static ssize_t proc_set_chan_plan(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct SetChannelPlan_param setChannelPlan_param;
	
	char tmp[32];
	u8 chan_plan = RT_CHANNEL_DOMAIN_REALTEK_DEFINE;
	
	if (!padapter)
		return -EFAULT;

	if (count < 1)
	{
		DBG_871X("argument size is less than 1\n");
		return -EFAULT;
	}	

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhx", &chan_plan);

		if (num !=  1) {
			DBG_871X("invalid read_reg parameter!\n");
			return count;
		}

	}
	setChannelPlan_param.channel_plan = chan_plan;
	if( H2C_SUCCESS != set_chplan_hdl(padapter, (unsigned char *)&setChannelPlan_param) )
		return -EFAULT;
		
	return count;

}

#ifdef CONFIG_DFS_MASTER
ssize_t proc_set_update_non_ocp(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *mlme = &adapter->mlmepriv;
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;
	char tmp[32];
	u8 ch, bw = CHANNEL_WIDTH_20, offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
	int ms = -1;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhu %hhu %hhu %d", &ch, &bw, &offset, &ms);

		if (num < 1 || (bw != CHANNEL_WIDTH_20 && num < 3))
			goto exit;

		if (bw == CHANNEL_WIDTH_20)
			rtw_chset_update_non_ocp_ms(mlmeext->channel_set
				, ch, bw, HAL_PRIME_CHNL_OFFSET_DONT_CARE, ms);
		else
			rtw_chset_update_non_ocp_ms(mlmeext->channel_set
				, ch, bw, offset, ms);
	}

exit:
	return count;
}

ssize_t proc_set_radar_detect(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	char tmp[32];
	u8 fake_radar_detect_cnt = 0;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhu", &fake_radar_detect_cnt);

		if (num < 1)
			goto exit;

		rfctl->dbg_dfs_master_fake_radar_detect_cnt = fake_radar_detect_cnt;
	}

exit:
	return count;
}
#endif /* CONFIG_DFS_MASTER */

static int proc_get_udpport(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct recv_priv *precvpriv = &(padapter->recvpriv);

	DBG_871X_SEL_NL(m,"%d\n",precvpriv->sink_udpport);	
	return 0;
}
static ssize_t proc_set_udpport(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct recv_priv *precvpriv = &(padapter->recvpriv);
	int sink_udpport = 0;	
	char tmp[32];
	
	
	if (!padapter)
		return -EFAULT;

	if (count < 1)
	{
		DBG_871X("argument size is less than 1\n");
		return -EFAULT;
	}	

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%d", &sink_udpport);

		if (num !=  1) {
			DBG_871X("invalid input parameter number!\n");
			return count;
		}

	}
	precvpriv->sink_udpport = sink_udpport;
	
	return count;

}

static int proc_get_macid_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	u8 i;

	DBG_871X_SEL_NL(m, "max_num:%u\n", macid_ctl->num);
	DBG_871X_SEL_NL(m, "\n");

	DBG_871X_SEL_NL(m, "used:\n");
	dump_macid_map(m, &macid_ctl->used, macid_ctl->num);
	DBG_871X_SEL_NL(m, "\n");

	DBG_871X_SEL_NL(m, "%-3s %-3s %-4s %-4s"
		"\n"
		, "id", "bmc", "if_g", "ch_g"
	);

	for (i=0;i<macid_ctl->num;i++) {
		if (rtw_macid_is_used(macid_ctl, i))
			DBG_871X_SEL_NL(m, "%3u %3u %4d %4d"
				"\n"
				, i
				, rtw_macid_is_bmc(macid_ctl, i)
				, rtw_macid_get_if_g(macid_ctl, i)
				, rtw_macid_get_ch_g(macid_ctl, i)
			);
	}

	return 0;
}

static int proc_get_sec_cam(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;

	DBG_871X_SEL_NL(m, "sec_cap:0x%02x\n", cam_ctl->sec_cap);
	DBG_871X_SEL_NL(m, "flags:0x%08x\n", cam_ctl->flags);
	DBG_871X_SEL_NL(m, "\n");

	DBG_871X_SEL_NL(m, "max_num:%u\n", cam_ctl->num);
	DBG_871X_SEL_NL(m, "used:\n");
	dump_sec_cam_map(m, &cam_ctl->used, cam_ctl->num);
	DBG_871X_SEL_NL(m, "\n");

	DBG_871X_SEL_NL(m, "reg_scr:0x%04x\n", rtw_read16(adapter, 0x680));
	DBG_871X_SEL_NL(m, "\n");

	dump_sec_cam(m, adapter);

	return 0;
}

static ssize_t proc_set_sec_cam(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	char tmp[32] = {0};
	char cmd[4];
	u8 id;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		/* c <id>: clear specific cam entry */
		/* wfc <id>: write specific cam entry from cam cache */

		int num = sscanf(tmp, "%s %hhu", cmd, &id);

		if (num < 2)
			return count;

		if (id >= cam_ctl->num) {
			DBG_871X_LEVEL(_drv_err_, FUNC_ADPT_FMT" invalid id:%u\n", FUNC_ADPT_ARG(adapter), id);
			return count;
		}

		if (strcmp("c", cmd) == 0) {
			_clear_cam_entry(adapter, id);
			adapter->securitypriv.hw_decrypted = _FALSE; /* temporarily set this for TX path to use SW enc */
		} else if (strcmp("wfc", cmd) == 0) {
			write_cam_from_cache(adapter, id);
		}
	}

	return count;
}

static int proc_get_sec_cam_cache(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	u8 i;

	DBG_871X_SEL_NL(m, "SW sec cam cache:\n");
	dump_sec_cam_ent_title(m, 1);
	for (i = 0; i < cam_ctl->num; i++) {
		if (dvobj->cam_cache[i].ctrl != 0)
			dump_sec_cam_ent(m, &dvobj->cam_cache[i], i);
	}

	return 0;
}

static ssize_t proc_set_change_bss_chbw(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *mlme = &(adapter->mlmepriv);
	struct mlme_ext_priv *mlmeext = &(adapter->mlmeextpriv);
	char tmp[32];
	u8 ch, bw = CHANNEL_WIDTH_20, offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhu %hhu %hhu", &ch, &bw, &offset);

		if (num < 1 || (bw != CHANNEL_WIDTH_20 && num < 3))
			goto exit;

		if (check_fwstate(mlme, WIFI_AP_STATE) && check_fwstate(mlme, WIFI_ASOC_STATE))
			rtw_change_bss_chbw_cmd(adapter, RTW_CMDF_WAIT_ACK, ch, bw, offset);
	}

exit:
	return count;
}

#ifdef CONFIG_RF_GAIN_OFFSET
static int proc_get_kfree_flag(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct kfree_data_t *kfree_data = GET_KFREE_DATA(adapter);

	DBG_871X_SEL_NL(m, "0x%02x\n", kfree_data->flag);

	return 0;
}

static ssize_t proc_set_kfree_flag(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct kfree_data_t *kfree_data = GET_KFREE_DATA(adapter);
	char tmp[32] = {0};
	u8 flag;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhx", &flag);

		if (num < 1)
			return count;

		kfree_data->flag = flag;
	}

	return count;
}

static int proc_get_kfree_bb_gain(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	struct kfree_data_t *kfree_data = GET_KFREE_DATA(adapter);
	u8 i, j;

	for (j = 0; j < hal_data->NumTotalRFPath; j++) {
		for (i = 0; i < BB_GAIN_NUM; i++)
			DBG_871X_SEL(m, "%d ", kfree_data->bb_gain[i][j]);
		DBG_871X_SEL(m, "\n");
	}

	return 0;
}

static ssize_t proc_set_kfree_bb_gain(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	struct kfree_data_t *kfree_data = GET_KFREE_DATA(adapter);
	char tmp[3 * BB_GAIN_NUM] = {0};
	u8 path;
	s8 bb_gain[BB_GAIN_NUM];

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		char *c, *next;
		int i;

		next = tmp;
		c = strsep(&next, " \t");

		if (c == NULL || sscanf(c, "%hhu", &path) != 1)
			return count;

		if (path >= hal_data->NumTotalRFPath || path >= RF_PATH_MAX)
			return count;

		c = strsep(&next, " \t");

		for (i = 0; i < BB_GAIN_NUM; i++) {
			if (c == NULL)
				break;

			if (sscanf(c, "%hhd", &bb_gain[i]) != 1)
				break;

			c = strsep(&next, " \t");
		}

		i--;
		for (; i >= 0; i--)
			kfree_data->bb_gain[i][path] = bb_gain[i];

	}

	return count;
}

static int proc_get_kfree_thermal(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct kfree_data_t *kfree_data = GET_KFREE_DATA(adapter);

	DBG_871X_SEL(m, "%d\n", kfree_data->thermal);

	return 0;
}

static ssize_t proc_set_kfree_thermal(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct kfree_data_t *kfree_data = GET_KFREE_DATA(adapter);
	char tmp[32] = {0};
	s8 thermal;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhd", &thermal);

		if (num < 1)
			return count;

		kfree_data->thermal = thermal;
	}

	return count;
}

static ssize_t proc_set_tx_gain_offset(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter;
	char tmp[32] = {0};
	u8 rf_path;
	s8 offset;

	adapter = (_adapter *)rtw_netdev_priv(dev);
	if (!adapter)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		u8 write_value;
		int num = sscanf(tmp, "%hhu %hhd", &rf_path, &offset);

		if (num < 2)
			return count;

		DBG_871X("write rf_path:%u tx gain offset:%d\n", rf_path, offset);
		rtw_rf_set_tx_gain_offset(adapter, rf_path, offset);
	}

	return count;
}
#endif /* CONFIG_RF_GAIN_OFFSET */

#ifdef CONFIG_BT_COEXIST
ssize_t proc_set_btinfo_evt(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u8 btinfo[8];

	if (count < 6)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = 0;

		_rtw_memset(btinfo, 0, 8);
		
		num = sscanf(tmp, "%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx"
			, &btinfo[0], &btinfo[1], &btinfo[2], &btinfo[3]
			, &btinfo[4], &btinfo[5], &btinfo[6], &btinfo[7]);

		if (num < 6)
			return -EINVAL;

		btinfo[1] = num-2;

		rtw_btinfo_cmd(padapter, btinfo, btinfo[1]+2);
	}
	
	return count;
}
#endif
#ifdef CONFIG_AUTO_CHNL_SEL_NHM
static int proc_get_best_chan(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	u8 best_24g_ch = 0, best_5g_ch = 0;

	rtw_hal_get_odm_var(adapter, HAL_ODM_AUTO_CHNL_SEL, &(best_24g_ch), &(best_5g_ch));

	DBG_871X_SEL_NL(m, "Best 2.4G CH:%u\n", best_24g_ch);
	DBG_871X_SEL_NL(m, "Best 5G CH:%u\n", best_5g_ch);
	return 0;
}

static ssize_t  proc_set_acs(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u8 acs_satae = 0;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}
	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhu", &acs_satae);
		
		if (num < 1)
			return -EINVAL;

		if (1 == acs_satae)
			rtw_acs_start(padapter, _TRUE);
		else
			rtw_acs_start(padapter, _FALSE);

	}
	return count;
}
#endif

/*
* rtw_adapter_proc:
* init/deinit when register/unregister net_device
*/
const struct rtw_proc_hdl adapter_proc_hdls [] = {
	{"write_reg", proc_get_dummy, proc_set_write_reg},
	{"read_reg", proc_get_read_reg, proc_set_read_reg},
	{"adapters_status", proc_get_dump_adapters_status, NULL},
	{"fwstate", proc_get_fwstate, NULL},
	{"sec_info", proc_get_sec_info, NULL},
	{"mlmext_state", proc_get_mlmext_state, NULL},
	{"qos_option", proc_get_qos_option, NULL},
	{"ht_option", proc_get_ht_option, NULL},
	{"rf_info", proc_get_rf_info, NULL},
	{"scan_param", proc_get_scan_param, proc_set_scan_param},
	{"scan_abort", proc_get_scan_abort, NULL},	
#ifdef CONFIG_SCAN_BACKOP
	{"backop_flags_sta", proc_get_backop_flags_sta, proc_set_backop_flags_sta},
	{"backop_flags_ap", proc_get_backop_flags_ap, proc_set_backop_flags_ap},
#endif
	{"survey_info", proc_get_survey_info, proc_set_survey_info},
	{"ap_info", proc_get_ap_info, NULL},
	{"trx_info", proc_get_trx_info, proc_reset_trx_info},
	{"rate_ctl", proc_get_rate_ctl, proc_set_rate_ctl},
	{"dis_pwt_ctl", proc_get_dis_pwt, proc_set_dis_pwt},
	{"mac_qinfo", proc_get_mac_qinfo, NULL},
	{"macid_info", proc_get_macid_info, NULL},
	{"sec_cam", proc_get_sec_cam, proc_set_sec_cam},
	{"sec_cam_cache", proc_get_sec_cam_cache, NULL},
	{"suspend_info", proc_get_suspend_resume_info, NULL},
	{"wifi_spec",proc_get_wifi_spec,NULL},
#ifdef CONFIG_LAYER2_ROAMING
	{"roam_flags", proc_get_roam_flags, proc_set_roam_flags},
	{"roam_param", proc_get_roam_param, proc_set_roam_param},
	{"roam_tgt_addr", proc_get_dummy, proc_set_roam_tgt_addr},
#endif /* CONFIG_LAYER2_ROAMING */

#ifdef CONFIG_SDIO_HCI
	{"sd_f0_reg_dump", proc_get_sd_f0_reg_dump, NULL},
	{"sdio_local_reg_dump", proc_get_sdio_local_reg_dump, NULL},
#endif /* CONFIG_SDIO_HCI */

	{"fwdl_test_case", proc_get_dummy, proc_set_fwdl_test_case},
	{"del_rx_ampdu_test_case", proc_get_dummy, proc_set_del_rx_ampdu_test_case},
	{"wait_hiq_empty", proc_get_dummy, proc_set_wait_hiq_empty},

	{"mac_reg_dump", proc_get_mac_reg_dump, NULL},
	{"bb_reg_dump", proc_get_bb_reg_dump, NULL},
	{"rf_reg_dump", proc_get_rf_reg_dump, NULL},
	
#ifdef CONFIG_AP_MODE
	{"all_sta_info", proc_get_all_sta_info, NULL},
#endif /* CONFIG_AP_MODE */

#ifdef DBG_MEMORY_LEAK
	{"_malloc_cnt", proc_get_malloc_cnt, NULL},
#endif /* DBG_MEMORY_LEAK */

#ifdef CONFIG_FIND_BEST_CHANNEL
	{"best_channel", proc_get_best_channel, proc_set_best_channel},
#endif

	{"rx_signal", proc_get_rx_signal, proc_set_rx_signal},
	{"hw_info", proc_get_hw_status, NULL},

#ifdef CONFIG_80211N_HT
	{"ht_enable", proc_get_ht_enable, proc_set_ht_enable},
	{"bw_mode", proc_get_bw_mode, proc_set_bw_mode},
	{"ampdu_enable", proc_get_ampdu_enable, proc_set_ampdu_enable},
	{"rx_stbc", proc_get_rx_stbc, proc_set_rx_stbc},
	{"rx_ampdu", proc_get_rx_ampdu, proc_set_rx_ampdu},
	{"rx_ampdu_factor",proc_get_rx_ampdu_factor,proc_set_rx_ampdu_factor},
	{"rx_ampdu_density",proc_get_rx_ampdu_density,proc_set_rx_ampdu_density},
	{"tx_ampdu_density",proc_get_tx_ampdu_density,proc_set_tx_ampdu_density},	 
#endif /* CONFIG_80211N_HT */

	{"en_fwps", proc_get_en_fwps, proc_set_en_fwps},
	{"mac_rptbuf", proc_get_mac_rptbuf, NULL},

	//{"path_rssi", proc_get_two_path_rssi, NULL},
//	{"rssi_disp",proc_get_rssi_disp, proc_set_rssi_disp},

#ifdef CONFIG_BT_COEXIST
	{"btcoex_dbg", proc_get_btcoex_dbg, proc_set_btcoex_dbg},
	{"btcoex", proc_get_btcoex_info, NULL},
	{"btinfo_evt", proc_get_dummy, proc_set_btinfo_evt},
#endif /* CONFIG_BT_COEXIST */

#if defined(DBG_CONFIG_ERROR_DETECT)
	{"sreset", proc_get_sreset, proc_set_sreset},
#endif /* DBG_CONFIG_ERROR_DETECT */
	{"linked_info_dump",proc_get_linked_info_dump,proc_set_linked_info_dump},

#ifdef CONFIG_GPIO_API
	{"gpio_info",proc_get_gpio,proc_set_gpio},
	{"gpio_set_output_value",proc_get_dummy,proc_set_gpio_output_value},
	{"gpio_set_direction",proc_get_dummy,proc_set_config_gpio},
#endif

#ifdef CONFIG_DBG_COUNTER
	{"rx_logs", proc_get_rx_logs, NULL},
	{"tx_logs", proc_get_tx_logs, NULL},
	{"int_logs", proc_get_int_logs, NULL},
#endif

#ifdef CONFIG_PCI_HCI
	{"rx_ring", proc_get_rx_ring, NULL},
	{"tx_ring", proc_get_tx_ring, NULL},
#endif
#ifdef CONFIG_GPIO_WAKEUP
	{"wowlan_gpio_info", proc_get_wowlan_gpio_info,
		proc_set_wowlan_gpio_info},
#endif
#ifdef CONFIG_P2P_WOWLAN
	{"p2p_wowlan_info", proc_get_p2p_wowlan_info, NULL},
#endif
	{"chan_plan",proc_get_chan_plan,proc_set_chan_plan},
#ifdef CONFIG_DFS_MASTER
	{"dfs_master_test_case", proc_get_dfs_master_test_case, proc_set_dfs_master_test_case},
	{"update_non_ocp", proc_get_dummy, proc_set_update_non_ocp},
	{"radar_detect", proc_get_dummy, proc_set_radar_detect},
#endif
	{"new_bcn_max", proc_get_new_bcn_max, proc_set_new_bcn_max},
	{"sink_udpport",proc_get_udpport,proc_set_udpport},
#ifdef DBG_RX_COUNTER_DUMP
	{"dump_rx_cnt_mode",proc_get_rx_cnt_dump,proc_set_rx_cnt_dump},
#endif	
	{"change_bss_chbw", NULL, proc_set_change_bss_chbw},
#ifdef CONFIG_RF_GAIN_OFFSET
	{"tx_gain_offset", proc_get_dummy, proc_set_tx_gain_offset},
	{"kfree_flag", proc_get_kfree_flag, proc_set_kfree_flag},
	{"kfree_bb_gain", proc_get_kfree_bb_gain, proc_set_kfree_bb_gain},
	{"kfree_thermal", proc_get_kfree_thermal, proc_set_kfree_thermal},
#endif
#ifdef CONFIG_POWER_SAVING
	{"ps_info",proc_get_ps_info, NULL},
#endif
#ifdef CONFIG_TDLS
	{"tdls_info", proc_get_tdls_info, NULL},
#endif
	{"monitor", proc_get_monitor, proc_set_monitor},

#ifdef CONFIG_AUTO_CHNL_SEL_NHM
	{"acs", proc_get_best_chan, proc_set_acs},
#endif
#ifdef CONFIG_PREALLOC_RX_SKB_BUFFER
	{"rtkm_info", proc_get_rtkm_info, NULL}
#endif
	{"efuse_map", proc_get_efuse_map, NULL},
#ifdef CONFIG_IEEE80211W
	{"11w_tx_sa_query", proc_get_tx_sa_query, proc_set_tx_sa_query},
	{"11w_tx_deauth", proc_get_tx_deauth, proc_set_tx_deauth},
	{"11w_tx_auth", proc_get_tx_auth, proc_set_tx_auth},
#endif /* CONFIG_IEEE80211W */
};

const int adapter_proc_hdls_num = sizeof(adapter_proc_hdls) / sizeof(struct rtw_proc_hdl);

static int rtw_adapter_proc_open(struct inode *inode, struct file *file)
{
	ssize_t index = (ssize_t)PDE_DATA(inode);
	const struct rtw_proc_hdl *hdl = adapter_proc_hdls+index;

	return single_open(file, hdl->show, proc_get_parent_data(inode));
}

static ssize_t rtw_adapter_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t index = (ssize_t)PDE_DATA(file_inode(file));
	const struct rtw_proc_hdl *hdl = adapter_proc_hdls+index;
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = hdl->write;

	if (write)
		return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);

	return -EROFS;
}

static const struct file_operations rtw_adapter_proc_fops = {
	.owner = THIS_MODULE,
	.open = rtw_adapter_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rtw_adapter_proc_write,
};

int proc_get_odm_dbg_comp(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_odm_dbg_comp_msg(m, adapter);

	return 0;
}

ssize_t proc_set_odm_dbg_comp(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];

	u64 dbg_comp;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%llx", &dbg_comp);

		if (num != 1)
			return count;

		rtw_odm_dbg_comp_set(adapter, dbg_comp);
	}

	return count;
}

int proc_get_odm_dbg_level(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_odm_dbg_level_msg(m, adapter);

	return 0;
}

ssize_t proc_set_odm_dbg_level(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];

	u32 dbg_level;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%u", &dbg_level);

		if (num != 1)
			return count;

		rtw_odm_dbg_level_set(adapter, dbg_level);
	}

	return count;
}

int proc_get_odm_ability(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_odm_ability_msg(m, adapter);

	return 0;
}

ssize_t proc_set_odm_ability(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];

	u32 ability;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%x", &ability);

		if (num != 1)
			return count;

		rtw_odm_ability_set(adapter, ability);
	}

	return count;
}

int proc_get_odm_adaptivity(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_odm_adaptivity_parm_msg(m, padapter);

	return 0;
}

ssize_t proc_set_odm_adaptivity(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 TH_L2H_ini;
	u32 TH_L2H_ini_mode2;
	s8 TH_EDCCA_HL_diff;
	s8 TH_EDCCA_HL_diff_mode2;
	u8 EDCCA_enable;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%x %hhd %x %hhd %hhu", &TH_L2H_ini, &TH_EDCCA_HL_diff, &TH_L2H_ini_mode2, &TH_EDCCA_HL_diff_mode2, &EDCCA_enable);

		if (num != 5)
			return count;

		rtw_odm_adaptivity_parm_set(padapter, (s8)TH_L2H_ini, TH_EDCCA_HL_diff, (s8)TH_L2H_ini_mode2, TH_EDCCA_HL_diff_mode2, EDCCA_enable);
	}
	
	return count;
}

static char *phydm_msg = NULL;
#define PHYDM_MSG_LEN	80*24

int proc_get_phydm_cmd(struct seq_file *m, void *v)
{
	struct net_device *netdev;
	PADAPTER padapter;
	PHAL_DATA_TYPE pHalData;
	PDM_ODM_T phydm;


	netdev = m->private;
	padapter = (PADAPTER)rtw_netdev_priv(netdev);
	pHalData = GET_HAL_DATA(padapter);
	phydm = &pHalData->odmpriv;

	if (NULL == phydm_msg) {
		phydm_msg = rtw_zmalloc(PHYDM_MSG_LEN);
		if (NULL == phydm_msg)
			return -ENOMEM;

		phydm_cmd(phydm, NULL, 0, 0, phydm_msg, PHYDM_MSG_LEN);
	}

	DBG_871X_SEL(m, "%s\n", phydm_msg);

	rtw_mfree(phydm_msg, PHYDM_MSG_LEN);
	phydm_msg = NULL;

	return 0;
}

ssize_t proc_set_phydm_cmd(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *netdev;
	PADAPTER padapter;
	PHAL_DATA_TYPE pHalData;
	PDM_ODM_T phydm;
	char tmp[64] = {0};


	netdev = (struct net_device*)data;
	padapter = (PADAPTER)rtw_netdev_priv(netdev);
	pHalData = GET_HAL_DATA(padapter);
	phydm = &pHalData->odmpriv;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp))
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		if (NULL == phydm_msg) {
			phydm_msg = rtw_zmalloc(PHYDM_MSG_LEN);
			if (NULL == phydm_msg)
				return -ENOMEM;
		} else {
			_rtw_memset(phydm_msg, 0, PHYDM_MSG_LEN);
		}

		phydm_cmd(phydm, tmp, count, 1, phydm_msg, PHYDM_MSG_LEN);

		if (strlen(phydm_msg) == 0) {
			rtw_mfree(phydm_msg, PHYDM_MSG_LEN);
			phydm_msg = NULL;
		}
	}

	return count;
}

/*
* rtw_odm_proc:
* init/deinit when register/unregister net_device, along with rtw_adapter_proc
*/
const struct rtw_proc_hdl odm_proc_hdls [] = {
	{"dbg_comp", proc_get_odm_dbg_comp, proc_set_odm_dbg_comp},
	{"dbg_level", proc_get_odm_dbg_level, proc_set_odm_dbg_level},
	{"ability", proc_get_odm_ability, proc_set_odm_ability},
	{"adaptivity", proc_get_odm_adaptivity, proc_set_odm_adaptivity},
	{"cmd", proc_get_phydm_cmd, proc_set_phydm_cmd},
};

const int odm_proc_hdls_num = sizeof(odm_proc_hdls) / sizeof(struct rtw_proc_hdl);

static int rtw_odm_proc_open(struct inode *inode, struct file *file)
{
	ssize_t index = (ssize_t)PDE_DATA(inode);
	const struct rtw_proc_hdl *hdl = odm_proc_hdls+index;

	return single_open(file, hdl->show, proc_get_parent_data(inode));
}

static ssize_t rtw_odm_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	ssize_t index = (ssize_t)PDE_DATA(file_inode(file));
	const struct rtw_proc_hdl *hdl = odm_proc_hdls+index;
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = hdl->write;
	
	if (write)
		return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);

	return -EROFS;
}

static const struct file_operations rtw_odm_proc_fops = {
	.owner = THIS_MODULE,
	.open = rtw_odm_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rtw_odm_proc_write,
};

struct proc_dir_entry *rtw_odm_proc_init(struct net_device *dev)
{
	struct proc_dir_entry *dir_odm = NULL;
	struct proc_dir_entry *entry = NULL;
	_adapter	*adapter = rtw_netdev_priv(dev);
	ssize_t i;

	if (adapter->dir_dev == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	if (adapter->dir_odm != NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	dir_odm = rtw_proc_create_dir("odm", adapter->dir_dev, dev);
	if (dir_odm == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	adapter->dir_odm = dir_odm;

	for (i=0;i<odm_proc_hdls_num;i++) {
		entry = rtw_proc_create_entry(odm_proc_hdls[i].name, dir_odm, &rtw_odm_proc_fops, (void *)i);
		if (!entry) {
			rtw_warn_on(1);
			goto exit;
		}
	}

exit:
	return dir_odm;
}

void rtw_odm_proc_deinit(_adapter	*adapter)
{
	struct proc_dir_entry *dir_odm = NULL;
	int i;

	dir_odm = adapter->dir_odm;

	if (dir_odm == NULL) {
		rtw_warn_on(1);
		return;
	}

	for (i=0;i<odm_proc_hdls_num;i++)
		remove_proc_entry(odm_proc_hdls[i].name, dir_odm);

	remove_proc_entry("odm", adapter->dir_dev);

	adapter->dir_odm = NULL;

	if (phydm_msg) {
		rtw_mfree(phydm_msg, PHYDM_MSG_LEN);
		phydm_msg = NULL;
	}
}

struct proc_dir_entry *rtw_adapter_proc_init(struct net_device *dev)
{
	struct proc_dir_entry *drv_proc = get_rtw_drv_proc();
	struct proc_dir_entry *dir_dev = NULL;
	struct proc_dir_entry *entry = NULL;
	_adapter *adapter = rtw_netdev_priv(dev);
	u8 rf_type;
	ssize_t i;

	if (drv_proc == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	if (adapter->dir_dev != NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	dir_dev = rtw_proc_create_dir(dev->name, drv_proc, dev);
	if (dir_dev == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	adapter->dir_dev = dir_dev;

	for (i=0;i<adapter_proc_hdls_num;i++) {
		entry = rtw_proc_create_entry(adapter_proc_hdls[i].name, dir_dev, &rtw_adapter_proc_fops, (void *)i);
		if (!entry) {
			rtw_warn_on(1);
			goto exit;
		}
	}

	rtw_odm_proc_init(dev);

exit:
	return dir_dev;
}

void rtw_adapter_proc_deinit(struct net_device *dev)
{
	struct proc_dir_entry *drv_proc = get_rtw_drv_proc();
	struct proc_dir_entry *dir_dev = NULL;
	_adapter *adapter = rtw_netdev_priv(dev);
	int i;

	dir_dev = adapter->dir_dev;

	if (dir_dev == NULL) {
		rtw_warn_on(1);
		return;
	}

	for (i=0;i<adapter_proc_hdls_num;i++)
		remove_proc_entry(adapter_proc_hdls[i].name, dir_dev);

	rtw_odm_proc_deinit(adapter);

	remove_proc_entry(dev->name, drv_proc);

	adapter->dir_dev = NULL;
}

void rtw_adapter_proc_replace(struct net_device *dev)
{
	struct proc_dir_entry *drv_proc = get_rtw_drv_proc();
	struct proc_dir_entry *dir_dev = NULL;
	_adapter *adapter = rtw_netdev_priv(dev);
	int i;

	dir_dev = adapter->dir_dev;

	if (dir_dev == NULL) {
		rtw_warn_on(1);
		return;
	}

	for (i=0;i<adapter_proc_hdls_num;i++)
		remove_proc_entry(adapter_proc_hdls[i].name, dir_dev);

	rtw_odm_proc_deinit(adapter);

	remove_proc_entry(adapter->old_ifname, drv_proc);

	adapter->dir_dev = NULL;

	rtw_adapter_proc_init(dev);

}

#endif /* CONFIG_PROC_DEBUG */

