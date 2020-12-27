// TODO unstable
#define DWG_TYPE DWG_TYPE_ASSOCOSNAPPOINTREFACTIONPARAM
#include "common.c"

void
api_process (dwg_object *obj)
{
  int error, isnew;
  ASSOCACTIONPARAM_fields;
  ASSOCCOMPOUNDACTIONPARAM_fields;
  
  BITCODE_BS status;
  BITCODE_RC osnap_mode;
  BITCODE_BD param; // 40 -1.0

  Dwg_Version_Type dwg_version = obj->parent->header.version;
  //#ifdef DEBUG_CLASSES
  dwg_obj_assocosnappointrefactionparam *_obj = dwg_object_to_ASSOCOSNAPPOINTREFACTIONPARAM (obj);

  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, is_r2013, BS);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, aap_version, BL);
  CHK_ENTITY_UTF8TEXT (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, name);

  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, class_version, BS);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, bs1, BS);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, num_params, BL);
  CHK_ENTITY_HV   (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, params, num_params);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, has_child_param, B);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, child_status, BS);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, child_id, BL);
  CHK_ENTITY_H    (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, child_param);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, bl2, BL);
  CHK_ENTITY_H    (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, h330_3);

  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, status, BS);
  CHK_ENTITY_MAX  (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, status, BS, 7);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, osnap_mode, RS);
  CHK_ENTITY_TYPE (_obj, ASSOCOSNAPPOINTREFACTIONPARAM, param, BD);
  //#endif
}
