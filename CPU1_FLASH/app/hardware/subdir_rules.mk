################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
app/hardware/%.obj: ../app/hardware/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"D:/Engineering_software/ti/CCS/CCS2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -O2 --fp_mode=relaxed --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55" --include_path="C:/ti/C2000Ware_26_01_00_00" --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55/device" --include_path="C:/ti/C2000Ware_26_01_00_00/driverlib/f28p55x/driverlib/" --include_path="D:/Engineering_software/ti/CCS/CCS2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include" --define=DEBUG --define=_FLASH --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="app/hardware/$(basename $(<F)).d_raw" --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55/CPU1_FLASH/syscfg" --obj_directory="app/hardware" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


