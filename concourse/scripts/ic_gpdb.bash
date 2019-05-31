#!/bin/bash -l

set -eox pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
  cat > /opt/run_test.sh <<-EOF
		trap look4diffs ERR

		function look4diffs() {

		    diff_files=\`find .. -name regression.diffs\`

		    for diff_file in \${diff_files}; do
			if [ -f "\${diff_file}" ]; then
			    cat <<-FEOF

						======================================================================
						DIFF FILE: \${diff_file}
						----------------------------------------------------------------------

						\$(cat "\${diff_file}")

					FEOF
			fi
		    done
		    exit 1
		}
		source /usr/local/greenplum-db-devel/greenplum_path.sh
		if [ -f /opt/gcc_env.sh ]; then
		    source /opt/gcc_env.sh
		fi
		cd "\${1}/gpdb_src"
		source gpAux/gpdemo/gpdemo-env.sh
		make -s ${MAKE_TEST_COMMAND}
	EOF

	chmod a+x /opt/run_test.sh
}

function setup_gpadmin_user() {
    ./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TEST_OS"
}

function _main() {
    if [ -z "${MAKE_TEST_COMMAND}" ]; then
        echo "FATAL: MAKE_TEST_COMMAND is not set"
        exit 1
    fi

    if [ -z "$TEST_OS" ]; then
        echo "FATAL: TEST_OS is not set"
        exit 1
    fi

    case "${TEST_OS}" in
    centos|sles|ubuntu) ;; #Valid
    *)
      echo "FATAL: TEST_OS is set to an invalid value: $TEST_OS"
      echo "Configure TEST_OS to be centos, sles, or ubuntu"
      exit 1
      ;;
    esac

    # This ugly block exists since sles11 installs kerberos at a different path that is a test-only dependency
    if [ "$TEST_OS" == "sles" ]; then
      zypper addrepo --no-gpgcheck http://download.opensuse.org/distribution/11.4/repo/oss/ ossrepo
      zypper -n install krb5 krb5-client krb5-server
      cp /usr/lib/mit/sbin/krb5kdc /usr/sbin/
      export PATH=/usr/lib/mit/sbin/:$PATH
      # Kerberos upgrades perl, so we have to re-do the symlink. Ideally, we
      # wouldn't have a symlink at all, but this step is necessary as long as
      # we build against a different version of perl, and explicitly link to
      # that version.
      # See gpdb/src/pl/plperl/GNUmakefile line 22:
      # perl_lib := $(basename $(notdir $(wildcard $(perl_archlibexp)/CORE/perl[5-9]*.lib)))
      local libperl_path
      libperl_path="$(rpm -ql perl | grep libperl.so)"
      ln -sf "$libperl_path" /lib64/libperl.so || return 1
    fi

    time install_and_configure_gpdb
    time setup_gpadmin_user
    time make_cluster
    time gen_env
    time run_test

    if [ "${TEST_BINARY_SWAP}" == "true" ]; then
        time ./gpdb_src/concourse/scripts/test_binary_swap_gpdb.bash
    fi

    if [ "${DUMP_DB}" == "true" ]; then
        chmod 777 sqldump
        su gpadmin -c ./gpdb_src/concourse/scripts/dumpdb.bash
    fi
}

_main "$@"
