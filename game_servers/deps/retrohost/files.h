#ifndef LRHOST_FILES_H__
#define LRHOST_FILES_H__

extern const char *system_path;
extern const char *save_path;

extern struct retro_game_info *game_info;
extern struct retro_game_info_ext *game_info_ext;

bool files_load(const char *path, const struct retro_system_content_info_override *content_info);
void files_mkdirs();
void files_clean_up();

#endif // LRHOST_FILES_H__
