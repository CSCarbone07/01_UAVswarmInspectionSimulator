#!/bin/bash

RUNS=50
NUM_OF_AGENTS=(50)
COMMUNICATIONS_RANGE=(5)
ATTRACTION=(0 2 4 8 16 32)
REPULSION=(8)
LOCALPATH=$(pwd)/
for x in "${NUM_OF_AGENTS[@]}"
do
  for y in "${COMMUNICATIONS_RANGE[@]}"
  do
    for i in "${ATTRACTION[@]}"
    do
      for j in "${REPULSION[@]}"
      do
        for (( c=1; c<=$RUNS; c++ ))
        do  
            if test -f "${LOCALPATH}results/results_status_${x}_${y}_${i}_${j}_10.yaml"
            then
                echo "$c:" >> "${LOCALPATH}results/results_status_${x}_${y}_${i}_${j}_10.yaml"
            else
                echo "$c:" > "${LOCALPATH}results/results_status_${x}_${y}_${i}_${j}_10.yaml"
            fi
            if test -f "${LOCALPATH}results/results_randomChoice_${x}_${y}_${i}_${j}_10.yaml"
            then
                echo "$c:" >> "${LOCALPATH}results/results_randomChoice_${x}_${y}_${i}_${j}_10.yaml"
            else
                echo "$c:" > "${LOCALPATH}results/results_randomChoice_${x}_${y}_${i}_${j}_10.yaml"
            fi
            if test -f "${LOCALPATH}results/results_timing_${x}_${y}_${i}_${j}_10.yaml"
            then
                echo "$c:" >> "${LOCALPATH}results/results_timing_${x}_${y}_${i}_${j}_10.yaml"
            else
                echo "$c:" > "${LOCALPATH}results/results_timing_${x}_${y}_${i}_${j}_10.yaml"
            fi
            if test -f "${LOCALPATH}results/results_visitedCells_${x}_${y}_${i}_${j}_10.yaml"
            then
                echo "$c:" >> "${LOCALPATH}results/results_visitedCells_${x}_${y}_${i}_${j}_10.yaml"
            else
                echo "$c:" > "${LOCALPATH}results/results_visitedCells_${x}_${y}_${i}_${j}_10.yaml"
            fi
            echo "Run #$c with $i ATTRACTION and $j REPULSION" 

            cp RWinput_param_0_8_10.yaml RWparam_found_script_0_8_10.yaml
            sed -i -e "s/_SEED_/$c/g" RWparam_found_script_0_8_10.yaml
            sed -i -e "s/_NUM_OF_AGENTS_/$x/g" RWparam_found_script_0_8_10.yaml
            sed -i -e "s/_COMMUNICATIONS_RANGE_/$y/g" RWparam_found_script_0_8_10.yaml
            sed -i -e "s/_ATTRACTION_/$i/g" RWparam_found_script_0_8_10.yaml
            sed -i -e "s/_REPULSION_/$j/g" RWparam_found_script_0_8_10.yaml
            sed -Ei "s|_PATH_|$LOCALPATH|g" RWparam_found_script_0_8_10.yaml
            /home/cscarbone/SwarmSimulators/01_UAVswarmInspectionSimulator/release/build/MACPP -i RWparam_found_script_0_8_10.yaml
        done
      done
    done	
  done
done

: <<'END'
END
