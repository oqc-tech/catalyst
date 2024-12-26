// Copyright 2024 Xanadu Quantum Technologies Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <pybind11/eval.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> 
#include <string>

#include "Exception.hpp"

extern "C" size_t* counts(const char*,const char*,unsigned long,unsigned long,const char*);

std::string program = R"(
import os
from qcaas_client.client import OQCClient, QPUTask, CompilerConfig
from qcaas_client.compiler_config import QuantumResultsFormat, Tket, TketOptimizations, MetricsType
#optimisations.tket_optimizations = TketOptimizations.DefaultMappingPass
#optimisations = Tket()
optimisations = Tket(TketOptimizations.One)
#optimisations = Tket(TketOptimizations.Two)

RES_FORMAT = QuantumResultsFormat().binary_count()

try:
    auth_token = os.environ.get("OQC_AUTH_TOKEN")
    device = os.environ.get("OQC_DEVICE")
    url = os.environ.get("OQC_URL")
    client = OQCClient(url=url, authentication_token=auth_token)
    oqc_config = CompilerConfig(repeats=shots, 
                                repetition_period = 90e-6,
                                results_format=RES_FORMAT, 
                                metrics = MetricsType.OptimizedInstructionCount,
                                optimizations=optimisations)
    oqc_task = QPUTask(program=circuit, config=oqc_config)
    task_id = client.schedule_tasks(oqc_task,qpu_id=device)[0].task_id
    res = client.get_task_results(task_id,qpu_id=device)
    counts = res.result["cbits"]

except Exception as e:
    print(f"circuit: {circuit}")
    msg = str(e)
)";

[[gnu::visibility("default")]] size_t* counts(const char *_circuit, 
                                           const char *_device, 
                                           size_t shots,
                                           size_t num_qubits, 
                                           const char *_kwargs)
{
    namespace py = pybind11;
    using namespace py::literals;

    py::gil_scoped_acquire lock;

    auto locals = py::dict("circuit"_a = _circuit, 
                           "device"_a = _device, 
                           "kwargs"_a = _kwargs,
                           "shots"_a = shots, 
                           "msg"_a = "");

    py::exec(program, py::globals(), locals);

    auto &&msg = locals["msg"].cast<std::string>();
    RT_FAIL_IF(!msg.empty(), msg.c_str());

    py::dict results = locals["counts"];


    size_t *cont_vec=(size_t*)malloc(sizeof(size_t)*(1<<num_qubits));
    for(size_t i=0;i<(1<<num_qubits);i++){
        cont_vec[i] = 0;
    }
    for (auto item : results) {
        auto key = item.first;
        auto value = item.second;
        size_t idx = std::stoi(key.cast<std::string>(),nullptr,2);
        cont_vec[idx] = value.cast<size_t>();
    }
    return cont_vec;
}

PYBIND11_MODULE(oqc_python_module, m) {
    m.doc() = "oqc";  // Module documentation
    m.def("counts", &counts,
          "Execute an OQC task and retrieve counts",
          pybind11::arg("_circuit"), 
          pybind11::arg("_device"), 
          pybind11::arg("shots"),
          pybind11::arg("num_qubits"), 
          pybind11::arg("_kwargs"));
}
