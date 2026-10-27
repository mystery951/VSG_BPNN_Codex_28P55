################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
build-1952283503: ../c2000.syscfg
	@echo 'SysConfig - building file: "$<"'
	"D:/Engineering_software/ti/CCS/CCS2051/ccs/utils/sysconfig_1.27.1/sysconfig_cli.bat" -s "C:/ti/C2000Ware_26_01_00_00/.metadata/sdk.json" -d "F28P55x" -p "128PDT" -r "F28P55x_128PDT" --script "D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55/c2000.syscfg" -o "syscfg" --compiler ccs
	@echo 'Finished building: "$<"'
	@echo ' '

syscfg/board.c: build-1952283503 ../c2000.syscfg
syscfg/board.h: build-1952283503
syscfg/board.cmd.genlibs: build-1952283503
syscfg/board.opt: build-1952283503
syscfg/board.json: build-1952283503
syscfg/pinmux.csv: build-1952283503
syscfg/epwm.dot: build-1952283503
syscfg/adc.dot: build-1952283503
syscfg/c2000ware_libraries.cmd.genlibs: build-1952283503
syscfg/c2000ware_libraries.opt: build-1952283503
syscfg/c2000ware_libraries.c: build-1952283503
syscfg/c2000ware_libraries.h: build-1952283503
syscfg/clocktree.h: build-1952283503
syscfg: build-1952283503

syscfg/%.obj: ./syscfg/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"D:/Engineering_software/ti/CCS/CCS2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -O2 --fp_mode=relaxed --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55" --include_path="C:/ti/C2000Ware_26_01_00_00" --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55/device" --include_path="C:/ti/C2000Ware_26_01_00_00/driverlib/f28p55x/driverlib/" --include_path="D:/Engineering_software/ti/CCS/CCS2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include" --define=DEBUG --define=RAM --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="syscfg/$(basename $(<F)).d_raw" --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55/CPU1_RAM/syscfg" --obj_directory="syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"D:/Engineering_software/ti/CCS/CCS2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -O2 --fp_mode=relaxed --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55" --include_path="C:/ti/C2000Ware_26_01_00_00" --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55/device" --include_path="C:/ti/C2000Ware_26_01_00_00/driverlib/f28p55x/driverlib/" --include_path="D:/Engineering_software/ti/CCS/CCS2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include" --define=DEBUG --define=RAM --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" --include_path="D:/Project/CCS/Workspace_28P550/VSG_BPNN_Codex_28P55/CPU1_RAM/syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


