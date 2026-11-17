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

#include <dpp/exception.h>
#include <dpp/isa_detection.h>
#include <dpp/discordvoiceclient.h>
#include "../../dave/encryptor.h"
#include "enabled.h"

namespace dpp {

void discord_voice_client::on_disconnect() {

	time_t current_time = time(nullptr);

	/* times_looped is only reset once a session is established (see handle_frame), not by
	 * elapsed time: a reconnect that fails every few seconds (e.g. resume rejected with 1002)
	 * must still escalate to a full reconnection instead of looping forever.
	 */
	times_looped++;

	/* If we've looped 5 or more times, abort the loop. */
	if (terminating || times_looped >= 5) {
		log(dpp::ll_warning, "Reached max loops whilst attempting to read from the websocket. Starting a full reconnection.");
		owner->queue_work(1, reconnection_callback);
		return;
	}
	last_loop_time = current_time;

	ssl_connection::close();
	owner->start_timer([this](auto handle) {
		log(dpp::ll_debug, "Attempting to reconnect voice websocket " + std::to_string(channel_id) + " to wss://" + hostname + "...");
		owner->stop_timer(handle);
		cleanup();
		if (timer_handle) {
			owner->stop_timer(timer_handle);
			timer_handle = 0;
		}
		start = time(nullptr);
		setup();
		terminating = false;
		ssl_connection::connect();
		websocket_client::connect();
		run();
	}, 1);
}

void discord_voice_client::run() {
	ssl_connection::read_loop();
}

}
