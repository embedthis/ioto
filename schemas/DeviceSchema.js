/*
    DeviceSchema -- Core schema for the device-* databases
 */

import blend from '@/paks/js-blend'
import CloudSchema from './parts/CloudSchema.json5'
import OneTable from './parts/OneTable.json5'
import MetricSchema from './parts/MetricSchema.json5'

let Schema = blend({
    version: 'latest',
    description: 'Default Device Cloud Schema',
    process: { },
    models: { },
}, CloudSchema, MetricSchema, OneTable)

export default Schema