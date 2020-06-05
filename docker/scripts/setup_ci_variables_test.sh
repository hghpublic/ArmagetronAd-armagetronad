# sets up the CI_* variables we're using for local testing

CI_COMMIT_REF_PROTECTED=false
CI_COMMIT_REF_NAME= 
CI_COMMIT_BEFORE_SHA=0000000000000000000000000000000000000000 
CI_COMMIT_SHA=001f69b535aa16139b61ed0a3141169a06d19c20
CI_COMMIT_TAG=
CI_COMMIT_BRANCH= 
CI_DEFAULT_BRANCH=master
CI_MERGE_REQUEST_CHANGED_PAGE_PATHS=
CI_MERGE_REQUEST_ID=
CI_MERGE_REQUEST_SOURCE_BRANCH_NAME=
CI_MERGE_REQUEST_TARGET_BRANCH_NAME=

case $1 in
	raw)
	CI_COMMIT_REF_PROTECTED=
	CI_COMMIT_BEFORE_SHA=
	CI_COMMIT_SHA=
	CI_DEFAULT_BRANCH=
	;;
    tag)
	CI_COMMIT_REF_NAME=v0.2.7.3.4
	CI_COMMIT_TAG=v0.2.7.3.4
	;;
    alpha)
	CI_COMMIT_REF_PROTECTED=true
	CI_COMMIT_REF_NAME=legacy_0.2.8.3
	CI_COMMIT_BRANCH=legacy_0.2.8.3
	;;
    beta)
	CI_COMMIT_REF_PROTECTED=true
	CI_COMMIT_REF_NAME=beta_0.2.8.3
	CI_COMMIT_BRANCH=beta_0.2.8.3
	;;
    rc)
	CI_COMMIT_REF_PROTECTED=true
	CI_COMMIT_REF_NAME=release_0.2.8.3
	CI_COMMIT_BRANCH=release_0.2.8.3
	;;
    release)
	CI_COMMIT_REF_PROTECTED=true
	CI_COMMIT_REF_NAME=v0.2.7.3.4
	CI_COMMIT_TAG=v0.2.7.3.4
	;;
    merge)
	CI_COMMIT_REF_NAME=release_0.2.8.3 # CAN BE ANYTHING, merger chooses
	CI_MERGE_REQUEST_CHANGED_PAGE_PATHS=
	CI_MERGE_REQUEST_ID=
	CI_MERGE_REQUEST_SOURCE_BRANCH_NAME=
	CI_MERGE_REQUEST_TARGET_BRANCH_NAME=
	CI_MERGE_REQUEST_ID=60167617
	CI_MERGE_REQUEST_SOURCE_BRANCH_NAME=mr_test
	CI_MERGE_REQUEST_TARGET_BRANCH_NAME=master
	;;
	*)
	echo "NO VALID CONFIGURATION SELECTED, available: raw (no CI), alpha, beta, rc, release, tag (unprotected), merge"
	exit 1
	;;
esac

export CI_COMMIT_REF_PROTECTED
export CI_COMMIT_REF_NAME
export CI_COMMIT_BEFORE_SHA
export CI_COMMIT_SHA
export CI_COMMIT_TAG
export CI_COMMIT_BRANCH 
export CI_DEFAULT_BRANCH
export CI_MERGE_REQUEST_CHANGED_PAGE_PATHS
export CI_MERGE_REQUEST_ID
export CI_MERGE_REQUEST_SOURCE_BRANCH_NAME
export CI_MERGE_REQUEST_TARGET_BRANCH_NAME