if [ "$#" -lt 3 ]; then
    echo "Illegal number of parameters"
    echo "Usage: ./extract_stat.sh [DATA_DIR] [INFO_DIR] [OUT_NAME]"
    exit 1
fi

DATA_DIR=${1}
INFO_DIR=${2}
OUT_NAME=${3}

mkdir -p ${INFO_DIR}
echo 'benchmarks,percent_of_non_branch,prediction_accuracy,percent_of_spec_instructions' > ${INFO_DIR}/${OUT_NAME}.csv

for file in ${DATA_DIR}/*core.txt; do

bench=$(pwd | sed "s:/: :g" | sed "s/_/ /g" | awk '{print $7}')
PWD=$(pwd)

if [ "$bench" = "gap" ]
then
    trace=$(echo $file | sed -e 's/.trace.gz[^ ]*//g' | awk -F "/" '{print $NF}')
else
    trace=$(echo $file | sed -e 's/.champsimtrace.xz[^ ]*//g' | awk -F "/" '{print $NF}')
fi


NON_BRANCH=$(grep "NOT_BRANCH:" $file | awk '{print $3}' | sed -e 's/%//g')
PRED_ACC=$(grep "Branch Prediction Accuracy:" $file | awk '{print $6}' | sed -e 's/%//g')
SPEC_INST=$(grep "Percentage of speculative instructions:" $file | awk '{print $5*100}' | sed -e 's/%//g')
# cycles=$(grep "Finished" $file | awk '{print $7}')
# ipc=$(grep "Finished" $file | awk '{print $10}')

echo $trace,$NON_BRANCH,$PRED_ACC,$SPEC_INST >> ${INFO_DIR}/${OUT_NAME}.csv

done