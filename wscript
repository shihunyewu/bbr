# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('bbr', ['internet', 'config-store','stats'])
    module.source = [
        'helper/udp-bbr-helper.cc',
        'model/ack-frame.cc',
        'model/bandwidth.cc',
        'model/bandwidth-sampler.cc',
        'model/bbr-sender.cc',
        'model/connection-stats.cc',
        'model/general-loss-algorithm.cc',
        'model/interval.cc',
        'model/pacing-sender.cc',
        'model/packet-header.cc',
        'model/stop-waiting-frame.cc',
        'model/received-packet-manager.cc',
        'model/rtt-stats.cc',
        'model/send-algorithm-interface.cc',
        'model/sent-packet-manager.cc',
        'model/udp-bbr-receiver.cc',
        'model/udp-bbr-sender.cc',
        'model/unacked-packet-map.cc',
        'model/videocodecs/my-traces-reader.cc',
        'model/videocodecs/video-codecs.cc',
        ]

    module_test = bld.create_ns3_module_test_library('bbr')
    module_test.source = [
        'test/bbr-test-suite.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'bbr'
    headers.source = [
        'helper/udp-bbr-helper.h',
        'model/udp-bbr-sender.h',
        'model/udp-bbr-receiver.h',
        'model/videocodecs/my-traces-reader.h',
        'model/videocodecs/video-codecs.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

