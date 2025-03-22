/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include <string_view>
#include <stdexcept>
#include <fstream>
#include <vector>
#include "mmu.h"
#include "sim.h"
#include "../../include/common.h"

static std::vector<std::pair<reg_t, abstract_device_t *>> difftest_plugin_devices;
static std::vector<std::string> difftest_htif_args;
static std::vector<std::pair<reg_t, mem_t *>> difftest_mem(
	1, std::make_pair(reg_t(DRAM_BASE), new mem_t(MSIZE)));
static debug_module_config_t difftest_dm_config = {
	.progbufsize = 2,
	.max_sba_data_width = 0,
	.require_authentication = false,
	.abstract_rti = 0,
	.support_hasel = true,
	.support_abstract_csr_access = true,
	.support_abstract_fpr_access = true,
	.support_haltgroups = true,
	.support_impebreak = true};

static sim_t *s = nullptr;
static processor_t *p = nullptr;
static state_t *state = nullptr;

void sim_t::diff_init()
{
	p = get_core("0");
	state = p->get_state();
}

void sim_t::diff_mem_init(std::string_view filename, std::uint64_t img_size)
{	
	std::ifstream img(filename.data(), std::ios::binary);
	if (!img)
    {
        throw std::runtime_error("Failed to open image file");
    }

	std::vector<uint8_t> buf(img_size, 0);
	if (!img.read((char *)buf.data(), img_size))
	{
		throw std::runtime_error("Failed to read image file");
	}
		
	mmu_t *mmu = p->get_mmu();
	for (size_t i = 0; i < img_size; i++)
	{
		mmu->store<uint8_t>(MBASE + i, buf[i]);
	}
}

void sim_t::diff_step(uint64_t n)
{
	step(n);
}

void sim_t::diff_get_regs(void *diff_context)
{
	struct diff_context_t *ctx = (struct diff_context_t *)diff_context;
	for (uint32_t i = 0; i < NUM_REGS; i++)
	{
		ctx->gpr[i] = state->XPR[i];
	}
	ctx->pc = state->pc;
}

void sim_t::diff_set_regs(void *diff_context)
{
	struct diff_context_t *ctx = (struct diff_context_t *)diff_context;
	for (uint32_t i = 0; i < NUM_REGS; i++)
	{
		state->XPR.write(i, (sword_t)ctx->gpr[i]);
	}
	state->pc = ctx->pc;
}

__EXPORT void difftest_init(std::string_view img, std::uint64_t img_size)
{
	difftest_htif_args.push_back("");
	const char *isa = "RV32I";
	cfg_t cfg(/*default_initrd_bounds=*/std::make_pair((reg_t)0, (reg_t)0),
			  /*default_bootargs=*/nullptr,
			  /*default_isa=*/isa,
			  /*default_priv=*/DEFAULT_PRIV,
			  /*default_varch=*/DEFAULT_VARCH,
			  /*default_misaligned=*/false,
			  /*default_endianness*/ endianness_little,
			  /*default_pmpregions=*/0,
			  /*default_mem_layout=*/std::vector<mem_cfg_t>(),
			  /*default_hartids=*/std::vector<size_t>(1),
			  /*default_real_time_clint=*/false,
			  /*default_trigger_count=*/4);
	s = new sim_t(&cfg, false,
				  difftest_mem, difftest_plugin_devices, difftest_htif_args,
				  difftest_dm_config, nullptr, false, NULL,
				  false,
				  NULL,
				  true);
	s->diff_init();
	s->diff_mem_init(img, img_size);
}

__EXPORT void difftest_regcpy(diff_context_t *context, DIRECTION direction)
{
	if (direction == DIRECTION::DIFFTEST_TO_REF)
	{
		s->diff_set_regs(context);
	}
	else
	{
		s->diff_get_regs(context);
	}
}

__EXPORT void difftest_exec(uint64_t n)
{
	s->diff_step(n);
}
