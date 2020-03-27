/**
 * Copyright 2019 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include "kernel/oplib/oplib.h"
#include "pipeline/pipeline.h"
#include "operator/composite/composite.h"
#include "ir/signature.h"
#include "pynative/pynative_execute.h"
#include "utils/symbolic.h"
#include "pybind_api/api_register.h"
#include "pipeline/parse/python_adapter.h"
#include "utils/summary/event_writer.h"
#include "utils/config_manager.h"
#include "optimizer/parallel/context.h"
#include "optimizer/parallel/device_manager.h"
#include "optimizer/parallel/costmodel_context.h"
#include "device/gpu/distribution/collective_init.h"

namespace py = pybind11;

using FuncGraph = mindspore::FuncGraph;
using EnvInstance = mindspore::EnvInstance;
using ExecutorPy = mindspore::pipeline::ExecutorPy;
using Pipeline = mindspore::pipeline::Pipeline;
using PrimitivePy = mindspore::PrimitivePy;
using MetaFuncGraph = mindspore::MetaFuncGraph;
using EventWriter = mindspore::summary::EventWriter;
using OpLib = mindspore::kernel::OpLib;
using ParallelContext = mindspore::parallel::ParallelContext;
using CostModelContext = mindspore::parallel::CostModelContext;

// Interface with python
PYBIND11_MODULE(_c_expression, m) {
  m.doc() = "MindSpore c plugin";

  (void)py::class_<MetaFuncGraph, std::shared_ptr<MetaFuncGraph>>(*m, "MetaFuncGraph_")
    .def_readonly(mindspore::PYTHON_METAFUNCGRAPH_FLAG, &mindspore::MetaFuncGraph::parse_info_)
    .def(py::init<std::string&>());

  auto fns = mindspore::PybindDefineRegister::AllFuncs();
  for (auto& item : fns) {
    item.second(&m);
  }

  // Class Pipeline interface
  (void)py::class_<ExecutorPy, std::shared_ptr<ExecutorPy>>(m, "Executor_")
    .def_static("get_instance", &ExecutorPy::GetInstance, "Executor get_instance.")
    .def("__call__", &ExecutorPy::Run, py::arg("args"), py::arg("phase") = py::str(""), "Executor run function.")
    .def("del_net_res", &ExecutorPy::DelNetRes, py::arg("network_id") = py::str(""), "Delete network resource.")
    .def("get_func_graph", &ExecutorPy::GetFuncGraph, py::arg("phase") = py::str(""), "Get graph pointer.")
    .def("get_func_graph_proto", &ExecutorPy::GetFuncGraphProto, py::arg("phase") = py::str(""),
         py::arg("type") = py::str("onnx_ir"), "Get graph proto string by specifying ir type.")
    .def("compile", &ExecutorPy::Compile, py::arg("obj"), py::arg("args"), py::arg("phase") = py::str(""),
         py::arg("use_vm") = py::bool_(false), "Compile obj by executor.")
    .def("get_parameter_layout", &ExecutorPy::GetParameterLayout, py::arg("phase") = py::str("train"),
         "Get Parameter Tensor Layout Dictionary.")
    .def("get_strategy", &ExecutorPy::GetCNodeStrategy, py::arg("phase") = py::str("train"),
         "Get CNode Strategy Dictionary.")
    .def("get_allreduce_fusion", &ExecutorPy::GetAllreduceFusion, py::arg("phase") = py::str("train"),
         "Get Allreduce Fusion Dictionary.")
    .def("build_data_graph", &ExecutorPy::BuildDFGraph, py::arg("build_params"), py::arg("phase") = py::str("train"),
         py::arg("broadcast_params") = py::dict(), "Build data graph.")
    .def("has_compiled", &ExecutorPy::HasCompiled, py::arg("phase") = py::str(""), "get if cell compiled.")
    .def("run_init_graph", &ExecutorPy::RunInitGraph, "Run init Graph.");
  // Class Graph interface
  (void)py::class_<FuncGraph, mindspore::FuncGraphPtr>(m, "FuncGraph").def(py::init());

  (void)py::class_<EnvInstance, std::shared_ptr<EnvInstance>>(m, "EnvInstance_")
    .def_readonly(mindspore::PYTHON_ENVINSTANCE_FLAG, &mindspore::EnvInstance::parse_info_)
    .def(py::init());

  (void)m.def("generate_key", &mindspore::pipeline::GenerateKey, "Generate the function graph key.");
  (void)m.def("real_run_op", &mindspore::pynative::RunOp, "Run op pynatively.");
  (void)m.def("initialize_distribute", &mindspore::pipeline::InitDistribute, "Initialize for Distribute.")
    .def("init_ge", &mindspore::pipeline::InitGe, "Init GE");
  (void)m.def("reset_op_id", &mindspore::pipeline::ResetOpId, "Reset Operator Id");
  (void)m.def("init_hccl", &mindspore::pipeline::InitHccl, "Init Hccl");
  (void)m.def("finalize_ge", &mindspore::pipeline::FinalizeGe, "Finalize Ge");
  (void)m.def("finalize_hccl", &mindspore::pipeline::FinalizeHccl, "Finalize Hccl");
  (void)m.def("set_ge_option", &mindspore::pipeline::SetGeOption, "API for set ge option.");
  (void)m.def("verify_inputs_signature", &mindspore::pipeline::VerifyInputSignature, "Verify input signature.");
  (void)m.def("init_exec_dataset", &mindspore::pipeline::InitExecDataset, py::arg("queue_name"), py::arg("size"),
              py::arg("batch_size"), py::arg("types"), py::arg("shapes"), py::arg("input_indexs"),
              py::arg("phase") = py::str("dataset"), "Init and exec dataset.");
  (void)m.def("_set_dataset_mode_config", &mindspore::ConfigManager::SetDatasetModeConfig, "API for set dataset mode.");
  (void)m.def("export_graph", &mindspore::pipeline::ExportDFGraph, "Export Graph.");

  (void)py::class_<mindspore::MsContext, std::shared_ptr<mindspore::MsContext>>(m, "MSContext")
    .def_static("get_instance", &mindspore::MsContext::GetInstance, "Get ms context instance.")
    .def("get_backend_policy", &mindspore::MsContext::backend_policy, "Get backend policy.")
    .def("set_backend_policy", &mindspore::MsContext::set_backend_policy, "Set backend policy.")
    .def("get_execution_mode", &mindspore::MsContext::execution_mode, "Get execution mode.")
    .def("set_execution_mode", &mindspore::MsContext::set_execution_mode, "Set execution mode.")
    .def("set_precompile_only", &mindspore::MsContext::set_precompile_only, "Set enable precompile only.")
    .def("get_precompile_only", &mindspore::MsContext::precompile_only, "Get enable precompile only.")
    .def("get_device_target", &mindspore::MsContext::device_target, "Get device target.")
    .def("set_device_target", &mindspore::MsContext::set_device_target, "Set device target.")
    .def("get_device_id", &mindspore::MsContext::device_id, "Get device id.")
    .def("set_device_id", &mindspore::MsContext::set_device_id, "Set device id.")
    .def("open_tsd", &mindspore::MsContext::OpenTsd, "Open tdt dataset client.")
    .def("close_tsd", &mindspore::MsContext::CloseTsd, "Close tdt dataset client.")
    .def("set_hccl_flag", &mindspore::MsContext::set_enable_hccl, "Set enable hccl.")
    .def("get_hccl_flag", &mindspore::MsContext::enable_hccl, "Get whether to enable hccl.")
    .def("set_task_sink_flag", &mindspore::MsContext::set_enable_task_sink, "Set enable task sink.")
    .def("get_task_sink_flag", &mindspore::MsContext::enable_task_sink, "Get whether to enable task sink.")
    .def("get_save_graphs_flag", &mindspore::MsContext::save_graphs_flag, "Get whether to save graphs.")
    .def("set_save_graphs_flag", &mindspore::MsContext::set_save_graphs_flag, "Set whether to save graphs.")
    .def("get_ir_fusion_flag", &mindspore::MsContext::ir_fusion_flag, "Get whether to enable ir fusion.")
    .def("set_ir_fusion_flag", &mindspore::MsContext::set_ir_fusion_flag, "Set whether to enable ir fusion.")
    .def("get_auto_mixed_precision_flag", &mindspore::MsContext::auto_mixed_precision_flag,
         "Get whether to enable auto mixed precision.")
    .def("set_auto_mixed_precision_flag", &mindspore::MsContext::set_auto_mixed_precision_flag,
         "Set whether to enable auto mixed precision.")
    .def("get_enable_reduce_precision_flag", &mindspore::MsContext::enable_reduce_precision,
         "Get whether to enable reduce precision.")
    .def("set_enable_reduce_precision_flag", &mindspore::MsContext::set_enable_reduce_precision,
         "Set whether to enable reduce precision.")
    .def("get_save_graphs_path", &mindspore::MsContext::save_graphs_path, "Get save graphs path.")
    .def("set_save_graphs_path", &mindspore::MsContext::set_save_graphs_path, "Set save graphs path.")
    .def("get_loop_sink_flag", &mindspore::MsContext::loop_sink_flag, "Get whether to enable loop sink.")
    .def("set_loop_sink_flag", &mindspore::MsContext::set_loop_sink_flag, "Set whether to enable loop sink.")
    .def("get_enable_mem_reuse", &mindspore::MsContext::enable_mem_reuse, "Get whether to enable mem reuse.")
    .def("set_enable_mem_reuse", &mindspore::MsContext::set_enable_mem_reuse, "Set whether to enable mem reuse.")
    .def("get_save_ms_model_flag", &mindspore::MsContext::save_ms_model_flag, "Get whether to save ms model.")
    .def("set_save_ms_model_flag", &mindspore::MsContext::set_save_ms_model_flag, "Set whether to save ms model.")
    .def("get_save_ms_model_path", &mindspore::MsContext::save_ms_model_path, "Get path to save ms model.")
    .def("set_save_ms_model_path", &mindspore::MsContext::set_save_ms_model_path, "Set path to save ms model")
    .def("get_enable_gpu_summary", &mindspore::MsContext::enable_gpu_summary, "Get whether to enable gpu summary.")
    .def("set_enable_gpu_summary", &mindspore::MsContext::set_enable_gpu_summary, "Set whether to enable gpu summary.")
    .def("get_enable_dump", &mindspore::MsContext::enable_dump, "Get whether to enable dump.")
    .def("set_enable_dump", &mindspore::MsContext::set_enable_dump, "Set whether to enable dump.")
    .def("get_save_dump_path", &mindspore::MsContext::save_dump_path, "Get path to dump.")
    .def("set_save_dump_path", &mindspore::MsContext::set_save_dump_path, "Set path to dump.")
    .def("get_enable_dynamic_mem_pool", &mindspore::MsContext::enable_dynamic_mem_pool,
         "Get whether to enable dynamic mem pool.")
    .def("set_enable_dynamic_mem_pool", &mindspore::MsContext::set_enable_dynamic_mem_pool,
         "Set whether to enable dynamic mem pool.")
    .def("set_graph_memory_max_size", &mindspore::MsContext::set_graph_memory_max_size, "set graph memory max size.")
    .def("set_variable_memory_max_size", &mindspore::MsContext::set_variable_memory_max_size,
         "set variable memory max size");

  (void)py::class_<ParallelContext, std::shared_ptr<ParallelContext>>(m, "AutoParallelContext")
    .def_static("get_instance", &ParallelContext::GetInstance, "Get auto parallel context instance.")
    .def("get_device_num", &ParallelContext::device_num, "Get device num.")
    .def("set_device_num", &ParallelContext::set_device_num, "Set device num.")
    .def("get_device_num_is_set", &ParallelContext::device_num_is_set, "Get device num is set.")
    .def("get_global_rank", &ParallelContext::global_rank, "Get global rank.")
    .def("set_global_rank", &ParallelContext::set_global_rank, "Set global rank.")
    .def("get_global_rank_is_set", &ParallelContext::global_rank_is_set, "Get global rank is set.")
    .def("get_mirror_mean", &ParallelContext::mirror_mean, "Get mirror mean.")
    .def("set_mirror_mean", &ParallelContext::set_mirror_mean, "Set mirror mean.")
    .def("get_cast_before_mirror", &ParallelContext::cast_before_mirror, "Get cast before mirror.")
    .def("set_cast_before_mirror", &ParallelContext::set_cast_before_mirror, "Set cast before mirror.")
    .def("get_communication_backend", &ParallelContext::communication_backend, "Get communication backend.")
    .def("set_communication_backend", &ParallelContext::set_communication_backend, "Set communication backend.")
    .def("get_parallel_mode", &ParallelContext::parallel_mode, "Get parallel mode.")
    .def("set_parallel_mode", &ParallelContext::set_parallel_mode, "Set parallel mode.")
    .def("get_strategy_search_mode", &ParallelContext::strategy_search_mode, "Get strategy search mode.")
    .def("set_strategy_search_mode", &ParallelContext::set_strategy_search_mode, "Set strategy search mode.")
    .def("set_all_reduce_fusion_split_indices", &ParallelContext::set_all_reduce_fusion_split_indices,
         "Set all reduce fusion split indices.")
    .def("get_all_reduce_fusion_split_indices", &ParallelContext::all_reduce_fusion_split_indices,
         "Get all reduce fusion split indices.")
    .def("set_all_reduce_fusion_split_sizes", &ParallelContext::set_all_reduce_fusion_split_sizes,
         "Set all reduce fusion split sizes.")
    .def("get_all_reduce_fusion_split_sizes", &ParallelContext::all_reduce_fusion_split_sizes,
         "Get all reduce fusion split sizes.")
    .def("get_parameter_broadcast", &ParallelContext::parameter_broadcast, "Get parameter broadcast.")
    .def("get_parameter_broadcast_is_set", &ParallelContext::parameter_broadcast_is_set,
         "Get parameter broadcast is set.")
    .def("set_parameter_broadcast", &ParallelContext::set_parameter_broadcast, "Set parameter broadcast.")
    .def("reset", &ParallelContext::Reset, "Reset auto parallel context.");

  (void)py::class_<CostModelContext, std::shared_ptr<CostModelContext>>(m, "CostModelContext")
    .def_static("get_instance", &CostModelContext::GetInstance, "Get cost_model context instance.")
    .def("set_device_memory_capacity", &CostModelContext::set_device_memory_capacity,
         "Set the capacity of device memory.")
    .def("get_device_memory_capacity", &CostModelContext::device_memory_capacity, "Get the capacity of device memory.")
    .def("set_costmodel_alpha", &CostModelContext::set_costmodel_alpha,
         "Set the parameter cost_model_alpha of the DP algorithm.")
    .def("get_costmodel_alpha", &CostModelContext::costmodel_alpha,
         "Get the parameter cost_model_alpha of the DP algorithm.")
    .def("set_costmodel_beta", &CostModelContext::set_costmodel_beta,
         "Set the parameter cost_model_beta of the DP algorithm.")
    .def("get_costmodel_beta", &CostModelContext::costmodel_beta,
         "Get the parameter cost_model_beta of the DP algorithm.")
    .def("set_costmodel_gamma", &CostModelContext::set_costmodel_gamma,
         "Set the parameter cost_model_gamma of the DP algorithm")
    .def("get_costmodel_gamma", &CostModelContext::costmodel_gamma,
         "Get the parameter cost_model_gamma of the DP algorithm.")
    .def("set_simplify_cal", &CostModelContext::set_costmodel_simplify_cal,
         "Set the parameter cost_model_simplify_cal of the DP algorithm.")
    .def("get_simplify_cal", &CostModelContext::costmodel_simplify_cal,
         "Get the parameter cost_model_simplify_cal of the DP algorithm.")
    .def("set_costmodel_communi_threshold", &CostModelContext::set_costmodel_communi_threshold,
         "Set the parameter cost_model_communi_threshold of the DP algorithm.")
    .def("get_costmodel_communi_threshold", &CostModelContext::costmodel_communi_threshold,
         "Get the parameter cost_model_communi_threshold of the DP algorithm.")
    .def("set_costmodel_communi_const", &CostModelContext::set_costmodel_communi_const,
         "Set the parameter cost_model_communi_const of the DP algorithm.")
    .def("get_costmodel_communi_const", &CostModelContext::costmodel_communi_const,
         "Get the parameter cost_model_communi_const of the DP algorithm.")
    .def("set_costmodel_communi_bias", &CostModelContext::set_costmodel_communi_bias,
         "Set the parameter cost_model_communi_bias of the DP algorithm.")
    .def("get_costmodel_communi_bias", &CostModelContext::costmodel_communi_bias,
         "Get the parameter cost_model_communi_bias of the DP algorithm.")
    .def("set_costmodel_allreduce_fusion_times", &CostModelContext::set_costmodel_allreduce_fusion_times,
         "Set the parameter gradient AllReduce times.")
    .def("get_costmodel_allreduce_fusion_times", &CostModelContext::costmodel_allreduce_fusion_times,
         "Get the parameter gradient AllReduce times.")
    .def("set_tensor_slice_align_enable", &CostModelContext::set_tensor_slice_alignment_enable,
         "Set the parameter tensor_slice_align_enable in strategy generation.")
    .def("get_tensor_slice_align_enable", &CostModelContext::tensor_slice_alignment_enable,
         "Get the parameter tensor_slice_align_enable in strategy generation.")
    .def("set_tensor_slice_align_size", &CostModelContext::set_tensor_slice_alignment_size,
         "Set the parameter tensor_slice_size in strategy generation.")
    .def("get_tensor_slice_align_size", &CostModelContext::tensor_slice_alignment_size,
         "Get the parameter tensor_slice_size in strategy generation.")
    .def("set_not_fully_use_devices", &CostModelContext::set_not_fully_use_device,
         "Set the parameter not_fully_use_devices in the DP algorithm.")
    .def("get_not_fully_use_devices", &CostModelContext::not_fully_use_device,
         "Get the parameter not_fully_use_devices in the DP algorithm.")
    .def("set_elementwise_op_strategy_follow", &CostModelContext::set_elementwise_stra_follow,
         "Set the parameter elementwise_op_strategy_follow in the DP algorithm.")
    .def("get_elementwise_op_strategy_follow", &CostModelContext::elementwise_stra_follow,
         "Get the parameter elementwise_op_strategy_follow in the DP algorithm.")
    .def("reset_cost_model", &CostModelContext::ResetCostModel, "Reset the CostModelContext.")
    .def("reset_algo_parameters", &CostModelContext::ResetAlgoParameters, "Reset the AlgoParameters.");

  (void)py::module::import("atexit").attr("register")(py::cpp_function{[&]() -> void {
    // only in case that c++ calling python interface, ClearResAtexit should be called.
    if (mindspore::parse::python_adapter::IsPythonEnv()) {
      mindspore::pipeline::ClearResAtexit();

#ifdef ENABLE_MINDDATA
      py::module iterators = py::module::import("mindspore.dataset.engine.iterators");
      (void)iterators.attr("_cleanup")();
#endif
    }
  }});

  (void)py::class_<EventWriter, std::shared_ptr<EventWriter>>(m, "EventWriter_")
    .def(py::init<const std::string&>())
    .def("GetFileName", &EventWriter::GetFileName, "Get the file name.")
    .def("Open", &EventWriter::Open, "Open the write file.")
    .def("Write", &EventWriter::Write, "Write the serialize event.")
    .def("EventCount", &EventWriter::GetWriteEventCount, "Write event count.")
    .def("Flush", &EventWriter::Flush, "Flush the event.")
    .def("Close", &EventWriter::Close, "Close the write.")
    .def("Shut", &EventWriter::Shut, "Final close the write.");

  (void)py::class_<OpLib, std::shared_ptr<OpLib>>(m, "Oplib")
    .def(py::init())
    .def("reg_op", &OpLib::RegOp, "Register op info.");

  (void)m.def("init_gpu_collective", &mindspore::device::gpu::CollectiveInitializer::InitCollective,
              "Init gpu collective communication mode.");
  (void)m.def("finalize_gpu_collective", &mindspore::device::gpu::CollectiveInitializer::FinalizeCollective,
              "Finalize gpu collective communication mode.");
}