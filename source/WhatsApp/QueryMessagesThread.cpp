#include <windows.h>
#include <sstream>

#include "QueryMessagesThread.h"
#include "Message.h"
#include "../Exceptions/SQLiteException.h"
#include "../Libraries/SQLite/SQLiteDatabase.h"
#include "../Libraries/SQLite/sqlite3.h"

QueryMessagesThread::QueryMessagesThread(WhatsappDatabase &database, SQLiteDatabase &sqLiteDatabase, const std::string &chatId, std::vector<WhatsappMessage *> &messages)
	: database(database), sqLiteDatabase(sqLiteDatabase), chatId(chatId), messages(messages)
{
}

QueryMessagesThread::~QueryMessagesThread()
{
}

void QueryMessagesThread::interrupt()
{
	ThreadWindows::interrupt();
	sqlite3_interrupt(sqLiteDatabase.getHandle());
}

WhatsappMessage *QueryMessagesThread::findByMessageId(const std::string &messageId)
{
	for(std::vector<WhatsappMessage *>::iterator it = messages.begin(); it != messages.end(); ++it)
	{
		WhatsappMessage *message = *it;
		if (message->getMessageId() == messageId)
		{
			return message;
		}
	}

	return NULL;
}

void QueryMessagesThread::run()
{
	//const char *query = "SELECT messages.key_id, messages.key_remote_jid, messages.key_from_me, messages.status, messages.data, messages.timestamp, messages.media_url, messages.media_mime_type, messages.media_wa_type, messages.media_size, messages.media_name, messages.media_caption, messages.media_duration, messages.latitude, messages.longitude, messages.thumb_image, messages.remote_resource, messages.raw_data, message_thumbnails.thumbnail, messages_quotes.key_id, messages_links._id " \
	//			"FROM messages " \
	//			"LEFT JOIN message_thumbnails on messages.key_id = message_thumbnails.key_id " \
	//			"LEFT JOIN messages_quotes on messages.quoted_row_id > 0 AND messages.quoted_row_id = messages_quotes._id " \
	//			"LEFT JOIN messages_links on messages._id = messages_links.message_row_id " \
	//			"WHERE messages.key_remote_jid = ? " \
	//			"ORDER BY messages.timestamp asc";
	const char *query = "SELECT message_view.key_id, chat_view.raw_string_jid, message_view.from_me, message_view.status, message_view.text_data, message_view.timestamp, message_view.media_url, message_view.media_mime_type, message_view.media_mime_type, message_view.media_size, message_view.media_name, message_view.media_caption, message_view.media_duration, message_view.latitude, message_view.longitude, message_view.thumb_image, jid.raw_string, message_view.raw_data, message_thumbnail.thumbnail, message_quoted.key_id, message_link._id " \
				"FROM message_view " \
				"LEFT OUTER JOIN jid on message_view.sender_jid_row_id = jid._id " \
				"LEFT JOIN chat_view on message_view.chat_row_id = chat_view._id " \
				"LEFT JOIN message_thumbnail on message_view._id = message_thumbnail.message_row_id " \
				"LEFT JOIN message_quoted on message_view.quoted_row_id > 0 AND message_view.key_id = message_quoted.key_id " \
				"LEFT JOIN message_link on message_view._id = message_link.message_row_id " \
				"WHERE chat_view.raw_string_jid = ? " \
				"ORDER BY message_view.timestamp asc";

	sqlite3_stmt *res;
	if (sqlite3_prepare_v2(sqLiteDatabase.getHandle(), query, -1, &res, NULL) != SQLITE_OK)
	{
		throw SQLiteException("Could not load messages", sqLiteDatabase);
	}

	if (sqlite3_bind_text(res, 1, chatId.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
	{
		throw SQLiteException("Could not bind sql parameter", sqLiteDatabase);
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		if (!running)
		{
			break;
		}

		std::string messageId = sqLiteDatabase.readString(res, 0);
		std::string chatId = sqLiteDatabase.readString(res, 1);
		int fromMe = sqlite3_column_int(res, 2);
		int status = sqlite3_column_int(res, 3);
		std::string data = sqLiteDatabase.readString(res, 4);
		long long timestamp = sqlite3_column_int64(res, 5);
		std::string mediaUrl = sqLiteDatabase.readString(res, 6);
		std::string mediaMimeType = sqLiteDatabase.readString(res, 7);
		int mediaWhatsappType = sqlite3_column_int(res, 8);
		int mediaSize = sqlite3_column_int(res, 9);
		std::string mediaName = sqLiteDatabase.readString(res, 10);
		std::string mediaCaption = sqLiteDatabase.readString(res, 11);
		int mediaDuration = sqlite3_column_int(res, 12);
		double latitude = sqlite3_column_double(res, 13);
		double longitude = sqlite3_column_double(res, 14);
		const void *thumbImage = sqlite3_column_blob(res, 15);
		int thumbImageSize = sqlite3_column_bytes(res, 15);
		std::string remoteResource = sqLiteDatabase.readString(res, 16);
		const void *rawData = sqlite3_column_blob(res, 17);
		int rawDataSize = sqlite3_column_bytes(res, 17);
		const void *thumbnailData = sqlite3_column_blob(res, 18);
		int thumbnailDataSize = sqlite3_column_bytes(res, 18);
		std::string quotedMessageId = sqLiteDatabase.readString(res, 19);
		int linkId = sqlite3_column_int(res, 20);

		WhatsappMessage *quotedMessage = NULL;
		if (quotedMessageId.length() > 0)
		{
			quotedMessage = findByMessageId(quotedMessageId);
		}
		WhatsappMessage *message = new WhatsappMessage(messageId, chatId, fromMe == 1, status, data, timestamp, 0, 0, mediaUrl, mediaMimeType, mediaWhatsappType, mediaSize, mediaName, mediaCaption, mediaDuration, latitude, longitude, thumbImage, thumbImageSize, remoteResource, rawData, rawDataSize, thumbnailData, thumbnailDataSize, quotedMessage, linkId > 0);
		messages.push_back(message);
	}

	sqlite3_finalize(res);
}
