#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/WaitSet.h"

#include "idl/NodeJSTestTypeSupportImpl.h"

#include <iostream>

const int DOMAIN_ID = 32;

#ifdef SECURE
const char DDSSEC_PROP_IDENTITY_CA[] = "dds.sec.auth.identity_ca";
const char DDSSEC_PROP_IDENTITY_CERT[] = "dds.sec.auth.identity_certificate";
const char DDSSEC_PROP_IDENTITY_PRIVKEY[] = "dds.sec.auth.private_key";
const char DDSSEC_PROP_PERM_CA[] = "dds.sec.access.permissions_ca";
const char DDSSEC_PROP_PERM_GOV_DOC[] = "dds.sec.access.governance";
const char DDSSEC_PROP_PERM_DOC[] = "dds.sec.access.permissions";

void append(DDS::PropertySeq& props, const char* name, const std::string& value)
{
  const DDS::Property_t prop = {
    name, (std::string("file:") + value).c_str(), false /*propagate*/};
  const unsigned int len = props.length();
  props.length(len + 1);
  props[len] = prop;
}
#endif

void wait_for_match(const DDS::DataWriter_var& writer)
{
  DDS::StatusCondition_var condition = writer->get_statuscondition();
  condition->set_enabled_statuses(DDS::PUBLICATION_MATCHED_STATUS);

  DDS::WaitSet_var ws = new DDS::WaitSet;
  ws->attach_condition(condition);

  DDS::Duration_t timeout =
    { DDS::DURATION_INFINITE_SEC, DDS::DURATION_INFINITE_NSEC };

  DDS::ConditionSeq conditions;
  DDS::PublicationMatchedStatus matches = {0, 0, 0, 0, 0};

  while (true) {
    if (writer->get_publication_matched_status(matches) != ::DDS::RETCODE_OK) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("%N:%l: wait_for_match()")
                 ACE_TEXT(" ERROR: get_publication_matched_status failed!\n")));
      ACE_OS::exit(-1);
    }

    if (matches.current_count >= 1) {
      break;
    }
    if (ws->wait(conditions, timeout) != DDS::RETCODE_OK) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("%N:%l: wait_for_match()")
                 ACE_TEXT(" ERROR: wait failed!\n")));
      ACE_OS::exit(-1);
    }
  }
  ws->detach_condition(condition);
}

int ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  try {
    DDS::DomainParticipantFactory_var dpf =
      TheParticipantFactoryWithArgs(argc, argv);

    DDS::DomainParticipantQos qos;
    dpf->get_default_participant_qos(qos);

#ifdef SECURE
    // Enable Security
    const std::string dds_root(getenv("DDS_ROOT"));
    const std::string dds_certs(dds_root + "/tests/security/certs");
    if (TheServiceParticipant->get_security()) {
      DDS::PropertySeq& props = qos.property.value;
      append(props, DDSSEC_PROP_IDENTITY_CA,
        dds_certs + "/opendds_identity_ca_cert.pem");
      append(props, DDSSEC_PROP_PERM_CA,
        dds_certs + "/opendds_identity_ca_cert.pem");
      append(props, DDSSEC_PROP_PERM_GOV_DOC,
				"security/governance_signed.p7s");
      append(props, DDSSEC_PROP_IDENTITY_CERT,
        dds_certs + "/mock_participant_1/opendds_participant_cert.pem");
      append(props, DDSSEC_PROP_IDENTITY_PRIVKEY,
        dds_certs + "/mock_participant_1/opendds_participant_private_key.pem");
      append(props, DDSSEC_PROP_PERM_DOC,
				"security/pub_permissions_signed.p7s");
    } else {
      ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT(
        "%N:%l: main() ERROR: This is the secure publisher, but security is not enabled.\n"
        )), 1);
    }
#endif

    DDS::DomainParticipant_var participant =
      dpf->create_participant(DOMAIN_ID, qos, 0, 0);

    if (!participant) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l: main()")
                        ACE_TEXT(" ERROR: create_participant failed!\n")),
                       -1);
    }

    Mod::SampleTypeSupport_var ts = new Mod::SampleTypeSupportImpl;
    ts->register_type(participant, "");
    CORBA::String_var type = ts->get_type_name();

    DDS::Topic_var topic =
      participant->create_topic("topic",
                                type,
                                TOPIC_QOS_DEFAULT,
                                0, 0);

    if (!topic) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l: main()")
                        ACE_TEXT(" ERROR: create_topic failed!\n")),
                       -1);
    }

    // Create Publisher
    DDS::Publisher_var pub =
      participant->create_publisher(PUBLISHER_QOS_DEFAULT, 0, 0);

    if (!pub) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l: main()")
                        ACE_TEXT(" ERROR: create_publisher failed!\n")),
                       -1);
    }

    // Create DataWriter
    DDS::DataWriterQos dw_qos;
    pub->get_default_datawriter_qos(dw_qos);
    const DDS::Duration_t one = {1, 0}, five = {5, 0};
    dw_qos.latency_budget.duration = one;
    dw_qos.liveliness.lease_duration = five;
    DDS::DataWriter_var dw = pub->create_datawriter(topic, dw_qos, 0, 0);

    wait_for_match(dw);

    Mod::SampleDataWriter_var writer = Mod::SampleDataWriter::_narrow(dw);

    Mod::Sample sample;
    sample.id = 23;
    sample.data = "Hello, world\n";
    sample.enu = Mod::two;
    sample.enu2 = static_cast<Mod::MyEnum>(42); // invalid enumerator
    sample.bt.o = 254;
    sample.bt.us = 65500;
    sample.bt.s = 32700;
    sample.bt.ul = 3000000000;
    sample.bt.l = 100000;
    sample.bt.ull = 0xABCDEF0123456789ULL;
    sample.bt.ll = 5000000000LL;
    sample.bt.f = 2.17f;
    sample.bt.d = 3.14;
    ACE_CDR_LONG_DOUBLE_ASSIGNMENT(sample.bt.ld, 1.4142136l);
    sample.bt.b = true;
    sample.bt.c = 'x';
    sample.bt.str = "z012";
    sample.bt.wstr = L"abcde";
    sample.seq1.length(5);
    for (CORBA::ULong i = 0; i < 5; ++i) sample.seq1[i] = i * 45;
    sample.seq2.length(3);
    for (CORBA::ULong i = 0; i < 3; ++i) sample.seq2[i] = i + 32;
    sample.ns.length(2);
    sample.ns[0].length(4);
    sample.ns[0][0] = "string1";
    sample.ns[0][1] = "string2";
    sample.ns[0][2] = "string3";
    sample.ns[0][3] = "string4";
    sample.ns[1].length(1);
    sample.ns[1][0] = "string5";

    writer->write(sample, DDS::HANDLE_NIL);
    ACE_OS::sleep(1);

    ++sample.id;
    writer->write(sample, DDS::HANDLE_NIL);
    ACE_OS::sleep(1);

    participant->delete_contained_entities();
    dpf->delete_participant(participant);
    TheServiceParticipant->shutdown();

  } catch (...) {
    std::cerr << "ERROR!\n";
    return 1;
  }
  return 0;
}
