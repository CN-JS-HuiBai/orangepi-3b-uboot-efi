// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020, Linaro Limited
 *
 * Tests scmi_agent uclass and the SCMI drivers implemented in other
 * uclass devices probe when a SCMI server exposes resources.
 *
 * Note in test.dts the protocol@10 node in scmi node. Protocol 0x10 is not
 * implemented in U-Boot SCMI components but the implementation is expected
 * to not complain on unknown protocol IDs, as long as it is not used. Note
 * in test.dts tests that SCMI drivers probing does not fail for such an
 * unknown SCMI protocol ID.
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <reset.h>
#include <scmi_agent.h>
#include <scmi_agent-uclass.h>
#include <scmi_protocols.h>
#include <asm/scmi_test.h>
#include <dm/device-internal.h>
#include <dm/test.h>
#include <linux/kconfig.h>
#include <power/regulator.h>
#include <test/ut.h>

static int ut_assert_scmi_state_postprobe(struct unit_test_state *uts,
					  struct sandbox_scmi_agent *agent,
					  struct udevice *dev)
{
	struct sandbox_scmi_devices *scmi_devices;

	/* Device references to check context against test sequence */
	scmi_devices = sandbox_scmi_devices_ctx(dev);
	ut_assertnonnull(scmi_devices);
	ut_asserteq(2, scmi_devices->clk_count);
	ut_asserteq(1, scmi_devices->reset_count);
	ut_asserteq(2, scmi_devices->regul_count);

	/* State of the simulated SCMI server exposed */
	ut_asserteq(3, agent->clk_count);
	ut_assertnonnull(agent->clk);
	ut_asserteq(1, agent->reset_count);
	ut_assertnonnull(agent->reset);
	ut_asserteq(2, agent->voltd_count);
	ut_assertnonnull(agent->voltd);

	return 0;
}

static int load_sandbox_scmi_test_devices(struct unit_test_state *uts,
					  struct sandbox_scmi_agent **ctx,
					  struct udevice **dev)
{
	struct udevice *agent_dev;

	ut_assertok(uclass_get_device_by_name(UCLASS_SCMI_AGENT, "scmi",
					      &agent_dev));
	ut_assertnonnull(agent_dev);

	*ctx = sandbox_scmi_agent_ctx(agent_dev);
	ut_assertnonnull(*ctx);

	/* probe */
	ut_assertok(uclass_get_device_by_name(UCLASS_MISC, "sandbox_scmi",
					      dev));
	ut_assertnonnull(*dev);

	return ut_assert_scmi_state_postprobe(uts, *ctx, *dev);
}

static int release_sandbox_scmi_test_devices(struct unit_test_state *uts,
					     struct udevice *dev)
{
	/* un-probe */
	ut_assertok(device_remove(dev, DM_REMOVE_NORMAL));

	return 0;
}

/*
 * Test SCMI states when loading and releasing resources
 * related to SCMI drivers.
 */
static int dm_test_scmi_sandbox_agent(struct unit_test_state *uts)
{
	struct sandbox_scmi_agent *ctx;
	struct udevice *dev = NULL;
	int ret;

	ret = load_sandbox_scmi_test_devices(uts, &ctx, &dev);
	if (!ret)
		ret = release_sandbox_scmi_test_devices(uts, dev);

	return ret;
}
DM_TEST(dm_test_scmi_sandbox_agent, UT_TESTF_SCAN_FDT);

static int dm_test_scmi_base(struct unit_test_state *uts)
{
	struct udevice *agent_dev, *base;
	struct scmi_agent_priv *priv;
	u32 version, num_agents, num_protocols, impl_version;
	u32 attributes, agent_id;
	u8 *vendor, *agent_name, *protocols;
	int ret;

	/* preparation */
	ut_assertok(uclass_get_device_by_name(UCLASS_SCMI_AGENT, "scmi",
					      &agent_dev));
	ut_assertnonnull(agent_dev);
	ut_assertnonnull(priv = dev_get_uclass_plat(agent_dev));
	ut_assertnonnull(base = scmi_get_protocol(agent_dev,
						  SCMI_PROTOCOL_ID_BASE));

	/* version */
	ret = scmi_base_protocol_version(base, &version);
	ut_assertok(ret);
	ut_asserteq(priv->version, version);

	/* protocol attributes */
	ret = scmi_base_protocol_attrs(base, &num_agents, &num_protocols);
	ut_assertok(ret);
	ut_asserteq(priv->num_agents, num_agents);
	ut_asserteq(priv->num_protocols, num_protocols);

	/* discover vendor */
	ret = scmi_base_discover_vendor(base, &vendor);
	ut_assertok(ret);
	ut_asserteq_str(priv->vendor, vendor);
	free(vendor);

	/* message attributes */
	ret = scmi_base_protocol_message_attrs(base,
					       SCMI_BASE_DISCOVER_SUB_VENDOR,
					       &attributes);
	ut_assertok(ret);
	ut_assertok(attributes);

	/* discover sub vendor */
	ret = scmi_base_discover_sub_vendor(base, &vendor);
	ut_assertok(ret);
	ut_asserteq_str(priv->sub_vendor, vendor);
	free(vendor);

	/* impl version */
	ret = scmi_base_discover_impl_version(base, &impl_version);
	ut_assertok(ret);
	ut_asserteq(priv->impl_version, impl_version);

	/* discover agent (my self) */
	ret = scmi_base_discover_agent(base, 0xffffffff, &agent_id,
				       &agent_name);
	ut_assertok(ret);
	ut_asserteq(priv->agent_id, agent_id);
	ut_asserteq_str(priv->agent_name, agent_name);
	free(agent_name);

	/* discover protocols */
	ret = scmi_base_discover_list_protocols(base, &protocols);
	ut_asserteq(num_protocols, ret);
	ut_asserteq_mem(priv->protocols, protocols, sizeof(u8) * num_protocols);
	free(protocols);

	/*
	 * NOTE: Sandbox SCMI driver handles device-0 only. It supports setting
	 * access and protocol permissions, but doesn't allow unsetting them nor
	 * resetting the configurations.
	 */
	/* set device permissions */
	ret = scmi_base_set_device_permissions(base, agent_id, 0,
					       SCMI_BASE_SET_DEVICE_PERMISSIONS_ACCESS);
	ut_assertok(ret); /* SCMI_SUCCESS */
	ret = scmi_base_set_device_permissions(base, agent_id, 1,
					       SCMI_BASE_SET_DEVICE_PERMISSIONS_ACCESS);
	ut_asserteq(-ENOENT, ret); /* SCMI_NOT_FOUND */
	ret = scmi_base_set_device_permissions(base, agent_id, 0, 0);
	ut_asserteq(-EACCES, ret); /* SCMI_DENIED */

	/* set protocol permissions */
	ret = scmi_base_set_protocol_permissions(base, agent_id, 0,
						 SCMI_PROTOCOL_ID_CLOCK,
						 SCMI_BASE_SET_PROTOCOL_PERMISSIONS_ACCESS);
	ut_assertok(ret); /* SCMI_SUCCESS */
	ret = scmi_base_set_protocol_permissions(base, agent_id, 1,
						 SCMI_PROTOCOL_ID_CLOCK,
						 SCMI_BASE_SET_PROTOCOL_PERMISSIONS_ACCESS);
	ut_asserteq(-ENOENT, ret); /* SCMI_NOT_FOUND */
	ret = scmi_base_set_protocol_permissions(base, agent_id, 0,
						 SCMI_PROTOCOL_ID_CLOCK, 0);
	ut_asserteq(-EACCES, ret); /* SCMI_DENIED */

	/* reset agent configuration */
	ret = scmi_base_reset_agent_configuration(base, agent_id, 0);
	ut_asserteq(-EACCES, ret); /* SCMI_DENIED */
	ret = scmi_base_reset_agent_configuration(base, agent_id,
						  SCMI_BASE_RESET_ALL_ACCESS_PERMISSIONS);
	ut_asserteq(-EACCES, ret); /* SCMI_DENIED */
	ret = scmi_base_reset_agent_configuration(base, agent_id, 0);
	ut_asserteq(-EACCES, ret); /* SCMI_DENIED */

	return 0;
}

DM_TEST(dm_test_scmi_base, UT_TESTF_SCAN_FDT);

static int dm_test_scmi_clocks(struct unit_test_state *uts)
{
	struct sandbox_scmi_agent *agent;
	struct sandbox_scmi_devices *scmi_devices;
	struct udevice *agent_dev, *clock_dev, *dev;
	int ret_dev;
	int ret;

	ret = load_sandbox_scmi_test_devices(uts, &agent, &dev);
	if (ret)
		return ret;

	scmi_devices = sandbox_scmi_devices_ctx(dev);
	ut_assertnonnull(scmi_devices);

	/* Sandbox SCMI clock protocol has its own channel */
	ut_assertok(uclass_get_device_by_name(UCLASS_SCMI_AGENT, "scmi",
					      &agent_dev));
	ut_assertnonnull(agent_dev);
	clock_dev = scmi_get_protocol(agent_dev, SCMI_PROTOCOL_ID_CLOCK);
	ut_assertnonnull(clock_dev);
	ut_asserteq(0x14, sandbox_scmi_channel_id(clock_dev));

	/* Test SCMI clocks rate manipulation */
	ut_asserteq(333, agent->clk[0].rate);
	ut_asserteq(200, agent->clk[1].rate);
	ut_asserteq(1000, agent->clk[2].rate);

	ut_asserteq(1000, clk_get_rate(&scmi_devices->clk[0]));
	ut_asserteq(333, clk_get_rate(&scmi_devices->clk[1]));

	ret_dev = clk_set_rate(&scmi_devices->clk[1], 1088);
	ut_assert(!ret_dev || ret_dev == 1088);

	ut_asserteq(1088, agent->clk[0].rate);
	ut_asserteq(200, agent->clk[1].rate);
	ut_asserteq(1000, agent->clk[2].rate);

	ut_asserteq(1000, clk_get_rate(&scmi_devices->clk[0]));
	ut_asserteq(1088, clk_get_rate(&scmi_devices->clk[1]));

	/* restore original rate for further tests */
	ret_dev = clk_set_rate(&scmi_devices->clk[1], 333);
	ut_assert(!ret_dev || ret_dev == 333);

	/* Test SCMI clocks gating manipulation */
	ut_assert(!agent->clk[0].enabled);
	ut_assert(!agent->clk[1].enabled);
	ut_assert(!agent->clk[2].enabled);

	ut_asserteq(0, clk_enable(&scmi_devices->clk[1]));

	ut_assert(agent->clk[0].enabled);
	ut_assert(!agent->clk[1].enabled);
	ut_assert(!agent->clk[2].enabled);

	ut_assertok(clk_disable(&scmi_devices->clk[1]));

	ut_assert(!agent->clk[0].enabled);
	ut_assert(!agent->clk[1].enabled);
	ut_assert(!agent->clk[2].enabled);

	return release_sandbox_scmi_test_devices(uts, dev);
}
DM_TEST(dm_test_scmi_clocks, UT_TESTF_SCAN_FDT);

static int dm_test_scmi_resets(struct unit_test_state *uts)
{
	struct sandbox_scmi_agent *agent;
	struct sandbox_scmi_devices *scmi_devices;
	struct udevice *agent_dev, *reset_dev, *dev = NULL;
	int ret;

	ret = load_sandbox_scmi_test_devices(uts, &agent, &dev);
	if (ret)
		return ret;

	scmi_devices = sandbox_scmi_devices_ctx(dev);
	ut_assertnonnull(scmi_devices);

	/* Sandbox SCMI reset protocol doesn't have its own channel */
	ut_assertok(uclass_get_device_by_name(UCLASS_SCMI_AGENT, "scmi",
					      &agent_dev));
	ut_assertnonnull(agent_dev);
	reset_dev = scmi_get_protocol(agent_dev, SCMI_PROTOCOL_ID_RESET_DOMAIN);
	ut_assertnonnull(reset_dev);
	ut_asserteq(0x0, sandbox_scmi_channel_id(reset_dev));

	/* Test SCMI resect controller manipulation */
	ut_assert(!agent->reset[0].asserted);

	ut_assertok(reset_assert(&scmi_devices->reset[0]));
	ut_assert(agent->reset[0].asserted);

	ut_assertok(reset_deassert(&scmi_devices->reset[0]));
	ut_assert(!agent->reset[0].asserted);

	return release_sandbox_scmi_test_devices(uts, dev);
}
DM_TEST(dm_test_scmi_resets, UT_TESTF_SCAN_FDT);

static int dm_test_scmi_voltage_domains(struct unit_test_state *uts)
{
	struct sandbox_scmi_agent *agent;
	struct sandbox_scmi_devices *scmi_devices;
	struct dm_regulator_uclass_plat *uc_pdata;
	struct udevice *dev;
	struct udevice *regul0_dev;

	ut_assertok(load_sandbox_scmi_test_devices(uts, &agent, &dev));

	scmi_devices = sandbox_scmi_devices_ctx(dev);
	ut_assertnonnull(scmi_devices);

	/* Set/Get an SCMI voltage domain level */
	regul0_dev = scmi_devices->regul[0];
	ut_assert(regul0_dev);

	uc_pdata = dev_get_uclass_plat(regul0_dev);
	ut_assert(uc_pdata);

	ut_assertok(regulator_set_value(regul0_dev, uc_pdata->min_uV));
	ut_asserteq(agent->voltd[0].voltage_uv, uc_pdata->min_uV);

	ut_assert(regulator_get_value(regul0_dev) == uc_pdata->min_uV);

	ut_assertok(regulator_set_value(regul0_dev, uc_pdata->max_uV));
	ut_asserteq(agent->voltd[0].voltage_uv, uc_pdata->max_uV);

	ut_assert(regulator_get_value(regul0_dev) == uc_pdata->max_uV);

	/* Enable/disable SCMI voltage domains */
	ut_assertok(regulator_set_enable(scmi_devices->regul[0], false));
	ut_assertok(regulator_set_enable(scmi_devices->regul[1], false));
	ut_assert(!agent->voltd[0].enabled);
	ut_assert(!agent->voltd[1].enabled);

	ut_assertok(regulator_set_enable(scmi_devices->regul[0], true));
	ut_assert(agent->voltd[0].enabled);
	ut_assert(!agent->voltd[1].enabled);

	ut_assertok(regulator_set_enable(scmi_devices->regul[1], true));
	ut_assert(agent->voltd[0].enabled);
	ut_assert(agent->voltd[1].enabled);

	ut_assertok(regulator_set_enable(scmi_devices->regul[0], false));
	ut_assert(!agent->voltd[0].enabled);
	ut_assert(agent->voltd[1].enabled);

	return release_sandbox_scmi_test_devices(uts, dev);
}
DM_TEST(dm_test_scmi_voltage_domains, UT_TESTF_SCAN_FDT);
