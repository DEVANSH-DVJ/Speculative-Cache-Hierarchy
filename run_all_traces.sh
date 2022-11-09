if [ "$#" -lt 4 ]; then
    echo "Illegal number of parameters"
    echo "Usage: ./run_champsim.sh [BINARY] [N_WARM] [N_SIM] [OUT_DIR] [OPTION]"
    exit 1
fi

BINARY=${1}
N_WARM=${2}
N_SIM=${3}
OUT_DIR=${4}
OPTION=${5}

TRACES=$(cat spec_trace_list.txt)

for TRACE in ${TRACES}
do
    ./run_champsim.sh ${BINARY} ${N_WARM} ${N_SIM} ${TRACE} ${OUT_DIR} &
done