import inspect
import fnmatch
import re
import popart
import numpy as np


def filter_dict(dict_to_filter, fun):
    sig = inspect.signature(fun)
    filter_keys = [
        param.name for param in sig.parameters.values()
        if param.kind == param.POSITIONAL_OR_KEYWORD
    ]
    filtered_dict = {
        filter_key: dict_to_filter[filter_key]
        for filter_key in filter_keys if filter_key in dict_to_filter.keys()
    }
    return filtered_dict


def get_poplar_cpu_device():

    return popart.DeviceManager().createCpuDevice()


def get_ipu_model(compileIPUCode=True, numIPUs=1, tilesPerIPU=1216):

    options = {
        "compileIPUCode": compileIPUCode,
        'numIPUs': numIPUs,
        "tilesPerIPU": tilesPerIPU
    }
    return popart.DeviceManager().createIpuModelDevice(options)


def get_compute_sets_from_report(report):

    lines = report.split('\n')
    cs = [x for x in lines if re.search(r' OnTileExecute .*:', x)]
    cs = [":".join(x.split(":")[1:]) for x in cs]
    cs = [x.strip() for x in cs]
    return set(cs)


def check_whitelist_entries_in_compute_sets(cs_list, whitelist):

    result = True
    fail_list = []
    wl = [x + '*' for x in whitelist]
    for cs in cs_list:
        if len([x for x in wl if fnmatch.fnmatch(cs, x)]) == 0:
            fail_list += [cs]
            result = False
    if not result:
        print("Failed to match " + str(fail_list))
    return result


def check_compute_sets_in_whitelist_entries(cs_list, whitelist):

    result = True
    fail_list = []
    wl = [x + '*' for x in whitelist]
    for x in wl:
        if len([cs for cs in cs_list if fnmatch.fnmatch(cs, x)]) == 0:
            fail_list += [x]
            result = False
    if not result:
        print("Failed to match " + str(fail_list))
    return result


def check_all_compute_sets_and_list(cs_list, whitelist):

    return (check_whitelist_entries_in_compute_sets(cs_list, whitelist)
            and check_compute_sets_in_whitelist_entries(cs_list, whitelist))


def get_compute_set_regex_count(regex, cs_list):

    return len([cs for cs in cs_list if re.search(regex, cs)])
