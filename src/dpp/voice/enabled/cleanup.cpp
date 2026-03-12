/************************************************************************************
 *
 * D++, A Lightweight C++ library for Discord
 *
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2021 Craig Edwards and D++ contributors
 * (https://github.com/brainboxdotcc/DPP/graphs/contributors)
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
 *
 ************************************************************************************/

#include <string_view>
#include <fstream>
#include <dpp/exception.h>
#include <dpp/isa_detection.h>
#include <dpp/discordvoiceclient.h>
#include <opus/opus.h>
#include "../../dave/encryptor.h"
#include "enabled.h"

namespace dpp {

void discord_voice_client::cleanup()
{
	constexpr int THREAD_JOIN_TIMEOUT = 2900; // milliseconds
	constexpr int THREAD_JOIN_SLEEP_INTERVAL = 10; // milliseconds
	if (encoder != nullptr) {
		opus_encoder_destroy(encoder);
		encoder = nullptr;
	}
	if (repacketizer != nullptr) {
		opus_repacketizer_destroy(repacketizer);
		repacketizer = nullptr;
	}
	{
		std::lock_guard lk(voice_courier_shared_state.mtx);
		voice_courier_shared_state.terminating = true;
	}
	voice_courier_shared_state.signal_iteration.notify_one();

	// wait until the thread becomes joinable (timeout to avoid permanent hang)
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(THREAD_JOIN_TIMEOUT);
	while (!voice_courier.joinable()) {
		if (std::chrono::steady_clock::now() > deadline) {
			log(dpp::ll_error, "voice courier thread did not become joinable within the timeout period, proceeding with cleanup anyway");
			break ;
		}
		// log(dpp::ll_info, "waiting for voice courier thread to become joinable...");
		std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_JOIN_SLEEP_INTERVAL));
	}
	if (voice_courier.joinable()) {
		voice_courier.join();
	}
	if (fd != INVALID_SOCKET) {
		owner->socketengine->delete_socket(fd);
	}
}

}
