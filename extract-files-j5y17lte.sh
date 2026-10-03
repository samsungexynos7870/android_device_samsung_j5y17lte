MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ORIGINAL_DIR="${PWD}"

ANDROID_ROOT="${MY_DIR}/../../.."
DEVICE_COMMON=universal7870-common
VENDOR=samsung
VENDOR_MK_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"
DEVICE_COMMON_ROOT="${ANDROID_ROOT}"/device/"${VENDOR}"/"${DEVICE_COMMON}"

TARGET_SOURCES_DIR="${VENDOR_MK_ROOT}/tmp/sources"
mkdir -p "$TARGET_SOURCES_DIR"

REPO_URLS=(
    "https://github.com/Exynos7870-labs/samsung_a6eltemtr_dump.git -b a6eltemtr-user-10-QP1A.190711.020-A600T1UVS8CUA1-release-keys A600T1UVS8CUA1"
    "https://github.com/Exynos7870-labs/samsung_j5y17lte_dump.git -b j5y17ltexx-user-9-PPR1.180610.011-J530FXXS8CUE4-release-keys J530FXXS8CUE4"
    "https://github.com/Exynos7870-labs/samsung_j5y17lte_dump.git -b j5y17ltexx-user-8.1.0-M1AJQ-J530FXXS5BSE3-release-keys J530FXXS5BSE3"
)

cd "$TARGET_SOURCES_DIR"
for i in "${!REPO_URLS[@]}"; do
    # Extract target directory name (last word in the string)
    repo_info="${REPO_URLS[$i]}"
    target_dir="${repo_info##* }"
    
    echo "Checking: $target_dir"
    
    if [[ -d "$target_dir" ]]; then
        echo "  Directory $target_dir already exists. Skipping."
    else
        echo "  Cloning: ${REPO_URLS[$i]}"
        git clone ${REPO_URLS[$i]}
    fi
done
ls
cd "$ORIGINAL_DIR"

COMMON_Q_A6ELTE_PATH="${TARGET_SOURCES_DIR}/A600T1UVS8CUA1"
COMMON_P_J5Y17LTE_PATH="${TARGET_SOURCES_DIR}/J530FXXS8CUE4"
COMMON_O_J5Y17LTE_PATH="${TARGET_SOURCES_DIR}/J530FXXS5BSE3"

# files
./extract-files.sh j5y17lte vendor-tools/proprietary-files_a6elte.txt -n -k $COMMON_Q_A6ELTE_PATH
./extract-files.sh j5y17lte vendor-tools/proprietary-files_j5y17lte_p.txt -n -k $COMMON_P_J5Y17LTE_PATH
./extract-files.sh j5y17lte vendor-tools/proprietary-files_j5y17lte_o.txt -n -k $COMMON_O_J5Y17LTE_PATH